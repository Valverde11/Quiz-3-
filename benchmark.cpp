
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "svg_plot.hpp"

using namespace std;
using Reloj = chrono::steady_clock;

static mt19937_64 rng(2103);  
static const double LIMITE_CACHE = (double)(1 << 17);

static const string DIR_RES = "resultados";
static const string DIR_GRAF = "graficas";

// Búsqueda binaria


// Versión para medir tiempo (sin contador)
int busquedaBinaria(const vector<int>& arr, int objetivo) {
    int izq = 0, der = (int)arr.size() - 1;
    while (izq <= der) {
        int medio = izq + (der - izq) / 2;
        if (arr[medio] == objetivo) return medio;
        if (arr[medio] < objetivo) izq = medio + 1;
        else der = medio - 1;
    }
    return -1;
}

// Versión para contar comparaciones 
int busquedaBinariaContada(const vector<int>& arr, int objetivo, long long& comps) {
    int izq = 0, der = (int)arr.size() - 1;
    while (izq <= der) {
        int medio = izq + (der - izq) / 2;
        comps++;
        if (arr[medio] == objetivo) return medio;
        if (arr[medio] < objetivo) izq = medio + 1;
        else der = medio - 1;
    }
    return -1;
}


// Mergesort
void mezclar(vector<int>& a, vector<int>& tmp, int izq, int medio, int der, long long& comps) {
    int i = izq, j = medio + 1, k = izq;
    while (i <= medio && j <= der) {
        comps++;
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else tmp[k++] = a[j++];
    }
    while (i <= medio) tmp[k++] = a[i++];
    while (j <= der) tmp[k++] = a[j++];
    for (int x = izq; x <= der; x++) a[x] = tmp[x];
}

void mergesortRec(vector<int>& a, vector<int>& tmp, int izq, int der, long long& comps) {
    if (izq >= der) return;
    int medio = izq + (der - izq) / 2;
    mergesortRec(a, tmp, izq, medio, comps);
    mergesortRec(a, tmp, medio + 1, der, comps);
    mezclar(a, tmp, izq, medio, der, comps);
}

long long mergesort(vector<int>& a) {
    vector<int> tmp(a.size());
    long long comps = 0;
    mergesortRec(a, tmp, 0, (int)a.size() - 1, comps);
    return comps;
}

// Resultados y ajuste teórico

struct Resultados {
    vector<double> n, tiempo, comps;  // tiempo en ns (búsqueda) o ms (mergesort)
};

struct Ajuste {
    double c, a, r2;  
};

// Mínimos cuadrados. Con intercepto=false ajusta y ≈ c·x (a = 0).
Ajuste ajustar(const vector<double>& x, const vector<double>& y, bool intercepto) {
    size_t m = x.size();
    double sx = 0, sy = 0, sxy = 0, sxx = 0;
    for (size_t i = 0; i < m; i++) { sx += x[i]; sy += y[i]; sxy += x[i] * y[i]; sxx += x[i] * x[i]; }
    double c, a;
    if (intercepto) {
        c = (m * sxy - sx * sy) / (m * sxx - sx * sx);
        a = (sy - c * sx) / m;
    } else {
        c = sxy / sxx;
        a = 0;
    }
    double media = sy / m, ssRes = 0, ssTot = 0;
    for (size_t i = 0; i < m; i++) {
        double p = c * x[i] + a;
        ssRes += (y[i] - p) * (y[i] - p);
        ssTot += (y[i] - media) * (y[i] - media);
    }
    return {c, a, 1.0 - ssRes / ssTot};
}

vector<double> mapear(const vector<double>& v, double (*f)(double)) {
    vector<double> r(v.size());
    for (size_t i = 0; i < v.size(); i++) r[i] = f(v[i]);
    return r;
}
double fLog2(double n) { return log2(n); }
double fNLog2(double n) { return n * log2(n); }
double fLineal(double n) { return n; }
double fCuadrado(double n) { return n * n; }

vector<double> escalar(const vector<double>& x, double c, double a = 0) {
    vector<double> r(x.size());
    for (size_t i = 0; i < x.size(); i++) r[i] = c * x[i] + a;
    return r;
}

string num(double v, const char* f = "%.4g") {
    char b[64];
    snprintf(b, sizeof b, f, v);
    return b;
}

void guardarCSV(const string& ruta, const string& colTiempo, const Resultados& r) {
    ofstream f(ruta);
    f << "n," << colTiempo << ",comparaciones_prom\n";
    for (size_t i = 0; i < r.n.size(); i++)
        f << (long long)r.n[i] << "," << r.tiempo[i] << "," << r.comps[i] << "\n";
}


// Benchmarks
Resultados benchBusquedaBinaria() {
    const int BUSQUEDAS = 1000000;  // búsquedas por repetición
    const int REPS = 9;             // repeticiones (se toma el mínimo)
    Resultados res;

    for (int k = 10; k <= 24; k++) {  // n = 2^10 ... 2^24
        int n = 1 << k;

        // Arreglo de n valores random, ordenado 
        uniform_int_distribution<int> dist(0, n * 8);
        vector<int> arr(n);
        for (auto& x : arr) x = dist(rng);
        sort(arr.begin(), arr.end());

        // Objetivos: 50 % existentes, 50 % random 
        vector<int> objetivos(BUSQUEDAS);
        uniform_int_distribution<int> idx(0, n - 1);
        for (int i = 0; i < BUSQUEDAS; i++)
            objetivos[i] = (i % 2 == 0) ? arr[idx(rng)] : dist(rng);

        // Tiempo 
        double mejor = 1e18;
        volatile long long sumidero = 0; 
        for (int r = 0; r < REPS; r++) {
            long long acum = 0;
            auto t0 = Reloj::now();
            for (int i = 0; i < BUSQUEDAS; i++) acum += busquedaBinaria(arr, objetivos[i]);
            auto t1 = Reloj::now();
            sumidero += acum;
            mejor = min(mejor, chrono::duration<double, nano>(t1 - t0).count() / BUSQUEDAS);
        }

        // Comparaciones 
        long long comps = 0;
        for (int i = 0; i < BUSQUEDAS; i++) busquedaBinariaContada(arr, objetivos[i], comps);
        double compsProm = (double)comps / BUSQUEDAS;

        res.n.push_back(n);
        res.tiempo.push_back(mejor);
        res.comps.push_back(compsProm);
        printf("[BinSearch] n=%9d  t=%8.1f ns  comps=%.2f\n", n, mejor, compsProm);
    }
    return res;
}

Resultados benchMergesort() {
    const int REPS = 7;
    const vector<int> tamanos = {10000,  20000,  50000,   100000,  200000,
                                 500000, 1000000, 2000000, 5000000};
    Resultados res;

    for (int n : tamanos) {
        double mejor = 1e18, sumaComps = 0;
        for (int r = 0; r < REPS; r++) {
            vector<int> arr(n);
            for (auto& x : arr) x = (int)(rng() % 1000000000ULL); 
            vector<int> copia = arr;

            auto t0 = Reloj::now();
            long long comps = mergesort(arr);
            auto t1 = Reloj::now();

            sort(copia.begin(), copia.end());  
            if (arr != copia) {
                cerr << "ERROR: mergesort produjo un resultado incorrecto\n";
                exit(1);
            }
            mejor = min(mejor, chrono::duration<double, milli>(t1 - t0).count());
            sumaComps += (double)comps;
        }
        double compsProm = sumaComps / REPS;
        res.n.push_back(n);
        res.tiempo.push_back(mejor);
        res.comps.push_back(compsProm);
        printf("[Mergesort] n=%8d  t=%9.2f ms  comps=%.0f\n", n, mejor, compsProm);
    }
    return res;
}


// Gráficas + resumen
Grafica nuevaGrafica(const string& titulo, const string& ey, bool logX) {
    Grafica g;
    g.titulo = titulo;
    g.etiquetaX = "Tamaño del arreglo (n)";
    g.etiquetaY = ey;
    g.logX = logX;
    return g;
}

void agregarPar(Grafica& g, const vector<double>& n, const vector<double>& emp,
                const vector<double>& teo, const string& etiquetaTeo) {
    Serie e; e.nombre = "Empírico"; e.x = n; e.y = emp; e.color = "#1f77b4";
    Serie t; t.nombre = etiquetaTeo; t.x = n; t.y = teo; t.color = "#ff7f0e";
    t.punteada = true; t.cuadrado = true;
    g.series = {e, t};
}

void guardar(const Grafica& g, const string& archivo) {
    string ruta = DIR_GRAF + "/" + archivo;
    if (!guardarSVG(g, ruta)) cerr << "No se pudo escribir " << ruta << "\n";
}

void generarGraficasYResumen(const Resultados& bb, const Resultados& ms) {
    vector<string> txt = {"Constante de ajuste y R² (empírico vs teórico)", string(60, '=')};
    vector<array<string, 4>> tabla;  // algoritmo, métrica, c, R²

    // ------------------------- Búsqueda binaria -------------------------
    vector<double> logn = mapear(bb.n, fLog2);

    // Comparaciones vs c·log2(n)
    Ajuste ac = ajustar(logn, bb.comps, false);
    {
        Grafica g = nuevaGrafica("Búsqueda binaria: comparaciones vs O(log n)", "Comparaciones promedio", true);
        agregarPar(g, bb.n, bb.comps, escalar(logn, ac.c), "Teórico  " + num(ac.c, "%.3f") + "·log₂(n)");
        guardar(g, "bb_comparaciones.svg");
    }
    txt.push_back("Búsqueda binaria (comparaciones)  c = " + num(ac.c) + "  R² = " + num(ac.r2, "%.5f"));
    tabla.push_back({"Búsqueda binaria", "Comparaciones", num(ac.c, "%.3g"), num(ac.r2, "%.5f")});

    // Tiempo: se ajusta solo con los n que caben en caché
    vector<double> nC, lC, tC;
    for (size_t i = 0; i < bb.n.size(); i++)
        if (bb.n[i] <= LIMITE_CACHE) { nC.push_back(bb.n[i]); lC.push_back(logn[i]); tC.push_back(bb.tiempo[i]); }
    if (nC.size() >= 3) {
        Ajuste at = ajustar(lC, tC, true);
        string etq = "Teórico  " + num(at.c, "%.2f") + "·log₂(n) " + num(at.a, "%+.1f");
        {
            Grafica g = nuevaGrafica("Búsqueda binaria: tiempo vs O(log n)", "Tiempo por búsqueda (ns, mínimo de 9 corridas)", true);
            agregarPar(g, nC, tC, escalar(lC, at.c, at.a), etq);
            guardar(g, "bb_tiempo.svg");
        }
        {
            Grafica g = nuevaGrafica("Búsqueda binaria: tiempo, todos los tamaños", "Tiempo por búsqueda (ns, mínimo de 9 corridas)", true);
            agregarPar(g, bb.n, bb.tiempo, escalar(logn, at.c, at.a), "Teórico (ajustado con n ≤ 2¹⁷)");
            guardar(g, "bb_tiempo_extendido.svg");
        }
        txt.push_back("Búsqueda binaria (tiempo, n <= 2^17) c = " + num(at.c) + "  a = " + num(at.a) +
                      "  R² = " + num(at.r2, "%.5f"));
        tabla.push_back({"Búsqueda binaria", "Tiempo (n ≤ 2¹⁷)", num(at.c, "%.3g") + " ns", num(at.r2, "%.5f")});
    }

    // ------------------------- Mergesort -------------------------
    vector<double> nlogn = mapear(ms.n, fNLog2);

    Ajuste mt = ajustar(nlogn, ms.tiempo, false);
    {
        Grafica g = nuevaGrafica("Mergesort: tiempo vs O(n log n)", "Tiempo (ms, mínimo de 7 corridas)", false);
        agregarPar(g, ms.n, ms.tiempo, escalar(nlogn, mt.c), "Teórico  " + num(mt.c, "%.2e") + "·n·log₂(n)");
        guardar(g, "ms_tiempo.svg");
    }
    txt.push_back("Mergesort (tiempo)                 c = " + num(mt.c) + "  R² = " + num(mt.r2, "%.5f"));
    tabla.push_back({"Mergesort", "Tiempo", num(mt.c, "%.3g") + " ms", num(mt.r2, "%.5f")});

    // Razón tiempo / (n log2 n): debe ser ~constante
    {
        vector<double> ratio(ms.n.size());
        double prom = 0;
        for (size_t i = 0; i < ratio.size(); i++) { ratio[i] = ms.tiempo[i] / nlogn[i] * 1e6; prom += ratio[i]; }
        prom /= ratio.size();
        Grafica g = nuevaGrafica("Mergesort: tiempo / (n·log₂ n) ≈ constante", "ms / (n·log₂ n) × 10⁶", true);
        g.yDesdeCero = true;
        g.yMaxMinimo = prom * 1.7;  // espacio para la leyenda
        Serie s; s.nombre = "Empírico"; s.x = ms.n; s.y = ratio;
        g.series = {s};
        g.tieneLineaH = true; g.lineaH = prom; g.etiquetaLineaH = "Promedio = " + num(prom, "%.3f");
        guardar(g, "ms_ratio_tiempo.svg");
    }

    Ajuste mc = ajustar(nlogn, ms.comps, false);
    {
        Grafica g = nuevaGrafica("Mergesort: comparaciones vs O(n log n)", "Comparaciones promedio", false);
        agregarPar(g, ms.n, ms.comps, escalar(nlogn, mc.c), "Teórico  " + num(mc.c, "%.3f") + "·n·log₂(n)");
        guardar(g, "ms_comparaciones.svg");
    }
    txt.push_back("Mergesort (comparaciones)          c = " + num(mc.c) + "  R² = " + num(mc.r2, "%.5f"));
    tabla.push_back({"Mergesort", "Comparaciones", num(mc.c, "%.3g"), num(mc.r2, "%.5f")});

    // ¿Qué modelo ajusta mejor a Mergesort?
    struct Modelo { string nombre; double (*f)(double); };
    vector<Modelo> modelos = {{"n", fLineal}, {"n·log₂n", fNLog2}, {"n²", fCuadrado}};
    vector<array<string, 3>> tablaModelos;
    txt.push_back("");
    txt.push_back("R² de Mergesort contra distintos modelos (comparaciones | tiempo)");
    for (auto& m : modelos) {
        vector<double> x = mapear(ms.n, m.f);
        double r2c = ajustar(x, ms.comps, false).r2, r2t = ajustar(x, ms.tiempo, false).r2;
        txt.push_back("  " + m.nombre + ": " + num(r2c, "%.5f") + " | " + num(r2t, "%.5f"));
        tablaModelos.push_back({m.nombre, num(r2c, "%.5f"), num(r2t, "%.5f")});
    }

    // resumen.txt y resumen.md (tablas listas para pegar en el README)
    ofstream ft(DIR_RES + "/resumen.txt");
    for (auto& l : txt) ft << l << "\n";
    ofstream fm(DIR_RES + "/resumen.md");
    fm << "| Algoritmo | Métrica | Constante `c` | R² |\n|---|---|---|---|\n";
    for (auto& r : tabla) fm << "| " << r[0] << " | " << r[1] << " | " << r[2] << " | " << r[3] << " |\n";
    fm << "\n| Modelo (Mergesort) | R² (comparaciones) | R² (tiempo) |\n|---|---|---|\n";
    for (auto& r : tablaModelos) fm << "| " << r[0] << " | " << r[1] << " | " << r[2] << " |\n";

    cout << "\n";
    for (auto& l : txt) cout << l << "\n";
}

int main() {
    filesystem::create_directories(DIR_RES);
    filesystem::create_directories(DIR_GRAF);

    Resultados bb = benchBusquedaBinaria();
    guardarCSV(DIR_RES + "/busqueda_binaria.csv", "tiempo_min_ns", bb);

    Resultados ms = benchMergesort();
    guardarCSV(DIR_RES + "/mergesort.csv", "tiempo_min_ms", ms);

    generarGraficasYResumen(bb, ms);
    cout << "\nListo. CSV y resumen en " << DIR_RES << "/, gráficas (.svg) en " << DIR_GRAF << "/\n";
    return 0;
}
