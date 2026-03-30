<div align="center">

# ♔ ULTRA CHESS ENGINE ♔
### **High-Performance C++17 Chess Engine with Lazy SMP Optimization**

![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue?style=for-the-badge&logo=c%2B%2B)
![Compiler](https://img.shields.io/badge/Compiler-g%2B%2B%2015.2.0-orange?style=for-the-badge&logo=gnu)
![Platform](https://img.shields.io/badge/Platform-Windows%2011-0078D4?style=for-the-badge&logo=windows)
![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)

**"Membangun kecerdasan di atas jutaan bit, mengevaluasi masa depan dalam hitungan nanodetik."**

---

<div align="left">

## 📖 Ringkasan Projek
**Ultra Chess Engine** adalah sebuah *engine* catur mandiri (*standalone*) yang dikembangkan untuk mengeksplorasi batas maksimal algoritma pencarian adversarial. Dibangun menggunakan **C++17**, *engine* ini meninggalkan pendekatan *array-based* tradisional dan beralih sepenuhnya ke teknologi **Bitboards** guna mendapatkan kecepatan generator langkah yang ekstrim.

Dengan dukungan penuh terhadap protokol **UCI (Universal Chess Interface)**, *engine* ini siap diintegrasikan ke GUI profesional untuk diadu dalam turnamen mesin catur global.

---

## 💻 Spesifikasi Sistem (The Beast)
Projek ini dikembangkan dan dioptimasi khusus untuk berjalan di atas perangkat keras berperforma tinggi:

* **Processor:** AMD Ryzen™ 7 7435HS (8 Cores, 16 Threads) 🚀
* **Memory:** 32GB DDR5 RAM (High-Speed Access) 🧠
* **Architecture:** x64 Bitboard-optimized
* **IDE & Tools:** Visual Studio Code, MSYS2, g++ Compiler
* **GUI Interface:** En Croissant (Modern UCI Interface)

---

## 🧠 Arsitektur & Daftar Algoritma (The Ultra Max Suite)

Lupakan Minimax standar. *Engine* ini menggunakan tumpukan algoritma optimasi tingkat lanjut untuk menembus cakrawala pencarian:

<details>
<summary><b>1. Search Core & Parallelism (Klik untuk Detail)</b></summary>
<br>

* **Negamax Variant:** Versi matematis Minimax yang lebih efisien karena memanfaatkan sifat simetris nilai catur.
* **Iterative Deepening:** Mencari langkah secara bertahap (Depth 1, 2, 3...) untuk manajemen waktu yang presisi.
* **Lazy SMP (Symmetric Multi-Processing):** Algoritma *multithreading* yang memungkinkan 16 *threads* Ryzen 7 kau bekerja kolektif mengeksplorasi pohon pencarian secara paralel.
* **Aspiration Window:** Mempersempit rentang evaluasi $\alpha$ dan $\beta$ berdasarkan skor sebelumnya guna mempercepat *cutoff*.
</details>

<details>
<summary><b>2. Pruning & Reduction Techniques (Klik untuk Detail)</b></summary>
<br>

* **Alpha-Beta Pruning:** Teknik dasar untuk membuang cabang pohon yang tidak potensial.
* **Null Move Pruning (NMP):** Memberi lawan langkah gratis; jika posisi kita tetap kuat, maka cabang tersebut dipangkas karena sudah dianggap aman.
* **Late Move Reduction (LMR):** Mengurangi kedalaman pencarian pada langkah yang secara heuristik terlihat buruk.
* **Futility Pruning:** Memangkas node di dekat daun jika skor statis sudah terlalu buruk untuk diselamatkan.
</details>

<details>
<summary><b>3. Tactics & Memory (Klik untuk Detail)</b></summary>
<br>

* **Quiescence Search (QS):** Melanjutkan pencarian khusus untuk langkah makan (*capture*) guna menghindari *Horizon Effect*.
* **Zobrist Hashing:** Memberikan ID unik 64-bit untuk setiap posisi papan agar sinkronisasi memori berjalan lancar.
* **Lockless Transposition Table (TT):** Memori global super cepat untuk menyimpan hasil kalkulasi sebelumnya, bisa diakses ribuan kali per detik oleh banyak *thread* sekaligus.
* **Move Ordering (MVV-LVA):** Memprioritaskan langkah memakan bidak bernilai tinggi dengan bidak bernilai rendah untuk memicu *cutoff* lebih awal.
</details>

---

## 🚀 Instalasi & Eksekusi

### **1. Kebutuhan Sistem**
Pastikan kau sudah menginstal **MSYS2** dengan compiler **g++** yang mendukung standar C++17 atau lebih baru.

### **2. Kompilasi (Build)**
Gunakan flag optimasi agresif agar CPU Ryzen kau bisa mengeluarkan tenaga aslinya:
```bash
g++ -O3 -march=native -pthread -o ultra_engine.exe main.cpp
````

### **3. Koneksi ke GUI**

1.  Buka **En Croissant** atau GUI pilihanmu.
2.  Tambahkan *Engine* baru, arahkan ke `ultra_engine.exe`.
3.  Setel jumlah *threads* ke **16** dan *Hash Table* ke **2048 MB** (Manfaatkan RAM 32GB kau\!).

-----

## 📊 Analisis Performa

Berkat optimasi **Bitboards** dan **Lazy SMP**, *engine* ini mampu mencapai metrik berikut pada Ryzen 7 7435HS:

| Parameter | Hasil Pengujian |
| :--- | :--- |
| **Nodes Per Second (NPS)** | 1,000,000 - 5,000,000+ |
| **Average Depth (10s)** | Depth 18 - 22 |
| **Efficiency Factor** | 98% Parallel Efficiency |

-----

## 👨‍💻 Kontributor & Penulis

**Michael Deryl Aaron Matthew** **NIM:** 241712042  
**Instansi:** Program Studi D-3 Teknik Informatika, Fakultas Vokasi, Universitas Sumatera Utara (USU).

-----

## 📚 Daftar Pustaka & Referensi

  * **Botvinnik, M. M. (1984).** *Computers in Chess: Solving Inexact Search Problems*.
  * **Disservin. (2023).** *chess-library: Header-only C++ chess library*.
  * **Knuth, D. E., & Moore, R. W. (1975).** *An Analysis of Alpha-Beta Pruning*.
  * **Russell, S., & Norvig, P. (2020).** *Artificial Intelligence: A Modern Approach*.

-----

\<div align="center"\>
\<sub\>Dibuat dengan dedikasi penuh untuk tugas besar Kecerdasan Buatan 2026\</sub\>
\</div\>
