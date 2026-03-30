#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include "chess.hpp"
#include <sstream>
#include <string>

using namespace chess;
using namespace std::chrono;

const int INF = 30000;
const int MATE_SCORE = 29000;
const int MAX_PLY = 100;
// Skor standar material: Pion, Kuda, Gajah, Benteng, Menteri, Raja
const int piece_values[6] = {100, 320, 330, 500, 900, 20000}; 

// Variabel global buat kontrol waktu & thread. Pake atomic biar aman diakses banyak thread (multithreading).
auto start_time = high_resolution_clock::now();
int opt_time = 0;
int max_time = 0;
std::atomic<bool> stop_search(false);
std::atomic<uint64_t> nodes_searched(0);

// Cek limit waktu. Cuma dicek tiap 2048 node (pake bitwise & 2047 biar operasi cepet) biar CPU ga capek ngecek jam terus.
void checkTime() {
    if (max_time > 0 && (nodes_searched & 2047) == 0) {
        auto now = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(now - start_time).count();
        if (duration > max_time) stop_search = true;
    }
}

// --- HEURISTICS (PANDUAN BIAR ENGINE NEBAK LANGKAH LEBIH PINTER) ---
// Killer Moves: Nyimpen langkah non-capture (ngga makan bidak) yang bikin beta-cutoff (langkah bagus) di ply tertentu.
int killer_moves[2][MAX_PLY]; 
// History Heuristic: Nyimpen seberapa sering sebuah langkah (dari kotak A ke kotak B) berhasil bikin cutoff secara historis.
int history_table[2][64][64]; 

void clearHeuristics() {
    for (int i=0; i<2; i++) {
        for (int j=0; j<MAX_PLY; j++) killer_moves[i][j] = 0;
        for (int j=0; j<64; j++) {
            for (int k=0; k<64; k++) history_table[i][j][k] = 0;
        }
    }
}

// --- TRANSPOSITION TABLE (TT) ---
// Cache (memori) buat nyimpen posisi papan yang udah pernah dihitung, biar ga ngitung ulang.
// Pake sistem "Lockless" buat Lazy SMP. Ada risiko tabrakan data (data race) antar thread, tapi di catur ini ditoleransi demi speed.
const int HASH_EXACT = 0, HASH_ALPHA = 1, HASH_BETA = 2, TT_UNKNOWN = 9999999;
struct TTEntry {
    uint64_t hash;
    uint16_t bestMove;
    int16_t score;
    int8_t depth;
    uint8_t flag;
    uint8_t age; // Buat ngeganti data lama (aging mechanic)
};
const int TT_SIZE = 4000000; 
std::vector<TTEntry> TTTable(TT_SIZE);
uint8_t current_age = 0;

// Penyesuaian skor skakmat berdasarkan kedalaman (ply).
// Engine bakal lebih milih skakmat yang lebih cepet (ply kecil) daripada skakmat yang lama.
int mateAdjustScore(int score, int ply) {
    if (score >= MATE_SCORE) return score - ply;
    if (score <= -MATE_SCORE) return score + ply;
    return score;
}
int mateUnadjustScore(int score, int ply) {
    if (score >= MATE_SCORE) return score + ply;
    if (score <= -MATE_SCORE) return score - ply;
    return score;
}

// Simpan data ke TT. Replace data kalau age-nya beda (posisi dari pencarian lama) atau depth pencarian sekarang lebih dalam.
void storeTT(uint64_t hash, int depth, int flag, int score, Move bestMove, int ply) {
    int index = hash % TT_SIZE;
    if (TTTable[index].hash == 0 || TTTable[index].age != current_age || TTTable[index].depth <= depth) {
        TTTable[index] = {hash, bestMove.move(), (int16_t)mateAdjustScore(score, ply), (int8_t)depth, (uint8_t)flag, current_age};
    }
}

// Cari data di TT. Kalau ketemu dan depth-nya cukup, balikin skornya biar ga usah masuk Minimax lagi.
int probeTT(uint64_t hash, int depth, int alpha, int beta, Move& ttMove, int ply) {
    int index = hash % TT_SIZE;
    if (TTTable[index].hash == hash) {
        ttMove = Move(TTTable[index].bestMove);
        if (TTTable[index].depth >= depth) {
            int score = mateUnadjustScore(TTTable[index].score, ply);
            if (TTTable[index].flag == HASH_EXACT) return score;
            if (TTTable[index].flag == HASH_ALPHA && score <= alpha) return alpha;
            if (TTTable[index].flag == HASH_BETA && score >= beta) return beta;
        }
    }
    return TT_UNKNOWN;
}

void clearTT() {
    for (int i = 0; i < TT_SIZE; ++i) TTTable[i].hash = 0;
}

// --- STATIC EVALUATION ---
// Fungsi buat ngasih nilai ke sebuah posisi papan (siapa yang lagi menang).
// Piece-Square Table (PST) ngasih bonus skor kalau bidak ada di posisi strategis (misal: pion di tengah lebih bagus).
const int pawn_pst[64] = { /* ... */ };
const int knight_pst[64] = { /* ... */ };

int evaluate(const Board& board) {
    int score = 0;
    int phase = 0; // Buat deteksi endgame (tapi belum kau pake penuh di sini)

    Bitboard white_pawns = board.pieces(PieceType::PAWN, Color::WHITE);
    Bitboard black_pawns = board.pieces(PieceType::PAWN, Color::BLACK);
    
    // Bonus punya 2 gajah (Bishop Pair lebih kuat di posisi terbuka)
    if (board.pieces(PieceType::BISHOP, Color::WHITE).count() >= 2) score += 40;
    if (board.pieces(PieceType::BISHOP, Color::BLACK).count() >= 2) score -= 40;

    for (int sq = 0; sq < 64; ++sq) {
        Piece p = board.at(Square(sq));
        if (p != Piece::NONE) {
            int pt = int(p.type());
            int val = piece_values[pt];
            int pst_val = 0;
            // Board flip otomatis buat hitam biar tabelnya bisa dipake dua warna
            int pst_idx = (p.color() == Color::WHITE) ? (sq ^ 56) : sq; 
            
            if (pt == int(PieceType::PAWN)) {
                pst_val = pawn_pst[pst_idx];
                // Bonus kasar buat Passed Pawn (pion bebas hambatan)
                if (p.color() == Color::WHITE && (sq / 8) >= 4) pst_val += 20;
                if (p.color() == Color::BLACK && (sq / 8) <= 3) pst_val += 20;
            }
            else if (pt == int(PieceType::KNIGHT)) pst_val = knight_pst[pst_idx];
            else if (pt == int(PieceType::ROOK)) {
                // Bonus benteng di Open File (lajur yang ga ada pion)
                if ((white_pawns & (0x0101010101010101ULL << (sq % 8))) == 0 && 
                    (black_pawns & (0x0101010101010101ULL << (sq % 8))) == 0) {
                    pst_val += 30;
                }
            }
            else if (pt == int(PieceType::KING)) {
                // King Safety basic: di opening/midgame, raja lebih aman di pojok.
                if (board.halfMoveClock() < 40) { 
                    if (sq % 8 <= 2 || sq % 8 >= 5) pst_val += 15; 
                }
            }

            if (pt != int(PieceType::PAWN) && pt != int(PieceType::KING)) phase++;

            if (p.color() == Color::WHITE) score += (val + pst_val);
            else score -= (val + pst_val);
        }
    }
    // Negamax perspective: skor harus positif buat sisi yang lagi jalan.
    return board.sideToMove() == Color::WHITE ? score : -score;
}

// --- MOVE ORDERING ---
// Alpha-Beta Pruning bakal jalan super cepet KLAU langkah bagus dicek duluan.
// Urutan prioritas: TT Move -> Makan bidak (MVV-LVA) -> Promosi -> Killer Moves -> History Heuristic.
void orderMoves(Movelist& moves, const Board& board, Move ttMove, int ply) {
    int color = int(board.sideToMove());
    for (auto& move : moves) {
        int score = 0;
        if (move == ttMove) {
            score = 1000000; 
        } else if (board.isCapture(move)) {
            // MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
            // Misal: Pion makan Menteri > Menteri makan pion.
            PieceType captured = board.at<PieceType>(move.to());
            PieceType attacker = board.at<PieceType>(move.from());
            if (captured != PieceType::NONE) score = 100000 + 10 * piece_values[int(captured)] - piece_values[int(attacker)];
        } else if (move.typeOf() == Move::PROMOTION) {
            score = 90000;
        } else {
            // Evaluasi langkah sunyi (Quiet moves)
            if (move.move() == killer_moves[0][ply]) score = 50000;
            else if (move.move() == killer_moves[1][ply]) score = 40000;
            else score = history_table[color][move.from().index()][move.to().index()];
        }
        move.setScore(score);
    }
    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {
        return a.score() > b.score();
    });
}

// --- QUIESCENCE SEARCH (QS) ---
// Buat nanganin Horizon Effect. Kalau kedalaman pencarian habis, tapi posisinya lagi kacau (ada bidak yang bisa dimakan),
// engine bakal lanjut nyari *khusus langkah capture* sampe posisinya tenang.
int quiescence(Board& board, int alpha, int beta, int ply) {
    nodes_searched++;
    if ((nodes_searched & 2047) == 0) checkTime();
    if (stop_search) return 0;
    if (ply >= MAX_PLY - 1) return evaluate(board);

    // Stand-pat: Asumsi paling aman adalah kita bisa skip giliran makan dan ambil skor statis saat ini.
    int stand_pat = evaluate(board);
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    Movelist moves;
    movegen::legalmoves<movegen::MoveGenType::CAPTURE>(moves, board); // Generate langkah capture doang
    orderMoves(moves, board, Move::NO_MOVE, ply); 

    for (const auto& move : moves) {
        board.makeMove(move);
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmakeMove(move);

        if (score >= beta) return beta; // Alpha-beta cutoff
        if (score > alpha) alpha = score;
    }
    return alpha;
}

// --- NEGAMAX CORE DENGAN TEKNIK PRUNING ---
// Varian dari Minimax, memanfaatkan fakta bahwa max(a, b) = -min(-a, -b). Lebih simpel penulisannya.
int minimax(Board& board, int depth, int alpha, int beta, int ply, bool isNullAllowed) {
    nodes_searched++;
    if ((nodes_searched & 2047) == 0) checkTime();
    if (stop_search) return 0;

    // Deteksi draw biar engine ga goblog muter-muter aja
    if (ply > 0 && (board.isRepetition() || board.halfMoveClock() >= 100)) return 0;
    if (ply >= MAX_PLY - 1) return evaluate(board);

    bool inCheck = board.inCheck();
    if (inCheck) depth++; // Check Extension: Kalau disekak, tambah kedalaman biar ga asal kabur dan mati.

    Move ttMove = Move::NO_MOVE;
    int ttVal = probeTT(board.hash(), depth, alpha, beta, ttMove, ply);
    if (ttVal != TT_UNKNOWN && ply > 0) return ttVal; 

    // Drop ke QS kalau depth habis
    if (depth <= 0) return quiescence(board, alpha, beta, ply);

    int static_eval = evaluate(board);

    // FUTILITY PRUNING / RAZORING
    // Kalau posisinya udah ampas banget dan depth tinggal dikit, skip pencarian dalam, langsung ke QS.
    if (!inCheck && depth == 1 && static_eval + 300 <= alpha) {
        return quiescence(board, alpha, beta, ply);
    }

    // NULL MOVE PRUNING (NMP)
    // Asumsi: "Kalau aku diam aja posisiku udah kuat, berarti aku gausah buang waktu nyari langkah di cabang ini."
    // Kita skip giliran sementara. Kalau musuh gagal nembus skor beta kita, cabang ini dipangkas (pruned).
    if (isNullAllowed && !inCheck && depth >= 3 && ply > 0 && static_eval >= beta) {
        board.makeNullMove();
        int nmpScore = -minimax(board, depth - 1 - 2, -beta, -beta + 1, ply + 1, false); // R = 2 (Reduction)
        board.unmakeNullMove();
        if (stop_search) return 0;
        if (nmpScore >= beta) return beta;
    }

    Movelist moves;
    movegen::legalmoves(moves, board);
    if (moves.empty()) {
        if (inCheck) return -INF + ply; // Kena Skakmat
        return 0; // Stalemate (Draw)
    }

    orderMoves(moves, board, ttMove, ply); 

    int bestScore = -INF;
    int flag = HASH_ALPHA;
    Move bestMoveThisNode = moves[0]; 
    int moves_searched = 0;

    for (const auto& move : moves) {
        // LATE MOVE PRUNING (LMP)
        // Langkah-langkah sisa (late moves) di kotak tenang kemungkinan besar jelek, skip aja kalau depth kecil.
        if (!inCheck && depth <= 2 && moves_searched > 15 && !board.isCapture(move) && move.typeOf() != Move::PROMOTION) {
            continue; 
        }

        board.makeMove(move);
        int score = 0;

        // PRINCIPAL VARIATION SEARCH (PVS) & LATE MOVE REDUCTION (LMR)
        if (moves_searched == 0) {
            // Langkah pertama diasumsikan sebagai yang terbaik (PV). Ditelusuri dengan window Alpha-Beta penuh.
            score = -minimax(board, depth - 1, -beta, -alpha, ply + 1, true);
        } else {
            // LMR: Kalau ini langkah kesekian dan kelihatannya aman, telusuri dengan depth dikurangi (-2) biar cepet.
            if (moves_searched >= 3 && depth >= 3 && !inCheck && !board.isCapture(move) && move.typeOf() != Move::PROMOTION) {
                score = -minimax(board, depth - 2, -alpha - 1, -alpha, ply + 1, true);
            } else {
                score = alpha + 1; // Hack buat maksa re-search kalau kondisi LMR ga terpenuhi.
            }

            // PVS: Tes pake window sangat sempit (null window: -alpha-1 sampai -alpha). 
            // Cuma ngetes "Apa langkah ini lebih bagus dari PV?". Kalau terbukti lebih bagus, baru full re-search.
            if (score > alpha) {
                score = -minimax(board, depth - 1, -alpha - 1, -alpha, ply + 1, true);
                if (score > alpha && score < beta) {
                    score = -minimax(board, depth - 1, -beta, -alpha, ply + 1, true);
                }
            }
        }
        board.unmakeMove(move);
        moves_searched++;

        if (stop_search) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMoveThisNode = move;
        }
        
        // Beta Cutoff (Move fail-high)
        if (bestScore >= beta) { 
            storeTT(board.hash(), depth, HASH_BETA, bestScore, bestMoveThisNode, ply);
            if (!board.isCapture(move)) {
                // Update Heuristics kalau ada langkah sunyi yang mematikan
                if (move.move() != killer_moves[0][ply]) {
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = move.move();
                }
                history_table[int(board.sideToMove())][move.from().index()][move.to().index()] += depth * depth;
            }
            return bestScore; 
        }
        if (bestScore > alpha) {
            flag = HASH_EXACT; // Update PV
            alpha = bestScore;
        }
    }
    
    storeTT(board.hash(), depth, flag, bestScore, bestMoveThisNode, ply);
    return bestScore;
}

// --- THREAD WORKER UNTUK LAZY SMP ---
// Lazy SMP = Symmetric Multiprocessing males. Semua thread nyari dari node root yang sama tanpa dibagi-bagi.
// Mereka eksplor cabang yang agak beda gara-gara race condition di Hash Table dan berbagi informasi via TT. 
void search_worker(Board board, int max_depth, int thread_id, Move& global_best_move) {
    int alpha = -INF;
    int beta = INF;
    int last_score = 0;

    // Iterative Deepening: Cari dari depth 1, 2, 3...
    // Biar kalau waktu habis di tengah jalan, setidaknya kita udah punya langkah terbaik dari depth sebelumnya.
    for (int d = 1; d <= max_depth; ++d) {
        // Aspiration window: Karena depth d - 1 udah tau skornya sekitar X, di depth d kita persempit batas atas dan bawahnya di sekitar X.
        // Kalau tebakan bener, pencarian jauh lebih cepet.
        if (d >= 4) { alpha = last_score - 50; beta = last_score + 50; }

        while (true) {
            int score = minimax(board, d, alpha, beta, 0, true);
            if (stop_search) break;

            // Kalau skor meleset dari Aspiration Window (Fail-low atau Fail-high), window dilebarin, trus re-search.
            if (score <= alpha) { alpha -= 200; continue; } 
            if (score >= beta) { beta += 200; continue; }   

            last_score = score;
            Move ttMove;
            probeTT(board.hash(), 0, -INF, INF, ttMove, 0);
            
            // Cuma thread 0 (main thread) yang lapor ke GUI via protokol UCI biar log-nya ga spam.
            if (thread_id == 0) {
                global_best_move = ttMove;
                auto now = high_resolution_clock::now();
                auto time_spent = duration_cast<milliseconds>(now - start_time).count();
                std::cout << "info depth " << d << " score cp " << score << " time " << time_spent << " nodes " << nodes_searched << " pv " << uci::moveToUci(global_best_move) << std::endl;
            }
            break; 
        }
        if (stop_search) break;
    }
}

// --- UCI PARSER ---
// Jembatan komunikasi antara engine (kode ini) dan GUI catur (kayak Arena, Lichess, dll) pakai standar teks.
int main() {
    Board board; 
    std::string line, token;
    std::setbuf(stdout, NULL); // Matiin buffering biar output lgsg nembak ke GUI
    clearTT();
    clearHeuristics();

    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        iss >> token;
        
        if (token == "uci") {
            std::cout << "id name AI_Catur_MAIN_FIX_JADI\n";
            std::cout << "id author Michael Deryl\n";
            std::cout << "uciok\n";
        } 
        else if (token == "isready") std::cout << "readyok\n";
        else if (token == "ucinewgame") {
            clearTT();
            clearHeuristics();
        }
        else if (token == "position") {
            // Setting posisi papan dari string FEN atau dari posisi awal + moves.
            std::string pos_type;
            iss >> pos_type;
            if (pos_type == "startpos") board.setFen(constants::STARTPOS);
            else if (pos_type == "fen") {
                std::string fen, fen_part;
                for(int i=0; i<6; i++) { if(iss >> fen_part) fen += fen_part + " "; }
                board.setFen(fen);
            }
            std::string moves_kw;
            while (iss >> moves_kw) {
                if (moves_kw == "moves") continue;
                Move m = uci::uciToMove(board, moves_kw);
                if (m != Move::NO_MOVE) board.makeMove(m);
            }
        } 
        else if (token == "go") {
            int wtime = 0, btime = 0, winc = 0, binc = 0, target_depth = 64; 
            std::string go_param;
            while (iss >> go_param) {
                if (go_param == "wtime") iss >> wtime;
                else if (go_param == "btime") iss >> btime;
                else if (go_param == "winc") iss >> winc;
                else if (go_param == "binc") iss >> binc;
                else if (go_param == "depth") iss >> target_depth;
            }
            
            // Time Management bodoh tapi jalan
            int time_left = (board.sideToMove() == Color::WHITE) ? wtime : btime;
            int inc = (board.sideToMove() == Color::WHITE) ? winc : binc;
            
            if (time_left > 0) {
                opt_time = (time_left / 40) + (inc / 2);
                max_time = time_left / 5; // Jangan lebih dari 20% total sisa waktu.
            } else {
                opt_time = 5000; max_time = 5000; 
            }

            start_time = high_resolution_clock::now();
            stop_search = false;
            nodes_searched = 0;
            current_age++; // Update age TT biar data lama ter-overwrite perlahan

            Move global_best_move = Move::NO_MOVE;
            
            // LAZY SMP: Langsung lempar thread buat nguli nyari langkah
            int num_threads = 4;
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; ++i) {
                threads.push_back(std::thread(search_worker, board, target_depth, i, std::ref(global_best_move)));
            }

            for (auto& th : threads) th.join();

            // Kalau entah kenapa ga dapet move sama sekali, paksa jalan move pertama yang legal biar ngga crash.
            if (global_best_move == Move::NO_MOVE) { 
                Movelist mlist; movegen::legalmoves(mlist, board);
                global_best_move = mlist[0];
            }
            std::cout << "bestmove " << uci::moveToUci(global_best_move) << std::endl;
        } 
        else if (token == "quit") {
            stop_search = true;
            break;
        }
    }
    return 0;
}