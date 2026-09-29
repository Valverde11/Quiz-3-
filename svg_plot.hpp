/*
 * svg_plot.hpp - Generador mínimo de gráficas de líneas en formato SVG.
 * Sin dependencias externas: solo la biblioteca estándar de C++17.
 * Los .svg se pueden ver en cualquier navegador y GitHub los muestra en el README.
 */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

struct Serie {
    std::string nombre;
    std::vector<double> x, y;
    std::string color = "#1f77b4";
    bool punteada = false;   // línea punteada (para la curva teórica)
    bool cuadrado = false;   // marcador cuadrado en vez de círculo
};

struct Grafica {
    std::string titulo, etiquetaX, etiquetaY;
    bool logX = false;          // eje X en escala log base 2
    bool yDesdeCero = false;    // forzar que el eje Y inicie en 0
    double yMaxMinimo = 0;      // el eje Y llega al menos hasta este valor
    std::vector<Serie> series;
    bool tieneLineaH = false;   // línea horizontal de referencia (ej. promedio)
    double lineaH = 0;
    std::string etiquetaLineaH;
};

namespace svg_detalle {

inline std::string esc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '&') r += "&amp;";
        else if (c == '<') r += "&lt;";
        else if (c == '>') r += "&gt;";
        else r += c;
    }
    return r;
}

inline std::string fmt(double v) {
    char b[32];
    double a = std::fabs(v);
    if (a == 0) return "0";
    if (a >= 1e9) std::snprintf(b, sizeof b, "%gG", v / 1e9);
    else if (a >= 1e6) std::snprintf(b, sizeof b, "%gM", v / 1e6);
    else if (a >= 1e4) std::snprintf(b, sizeof b, "%gk", v / 1e3);
    else std::snprintf(b, sizeof b, "%.4g", v);
    return b;
}

// Exponente con superíndices Unicode (se ve bien en cualquier visor)
inline std::string potencia2(int k) {
    static const char* sup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string r = "2", d = std::to_string(k);
    for (char c : d) r += sup[c - '0'];
    return r;
}

inline double numeroBonito(double rango, bool redondear) {
    double e = std::floor(std::log10(rango));
    double f = rango / std::pow(10, e), nf;
    if (redondear) nf = f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10;
    else nf = f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10;
    return nf * std::pow(10, e);
}

// Ticks "bonitos": actualiza lo/hi y retorna el paso
inline double ejeBonito(double& lo, double& hi, int ticks = 6) {
    if (hi <= lo) hi = lo + 1;
    double rango = numeroBonito(hi - lo, false);
    double paso = numeroBonito(rango / (ticks - 1), true);
    lo = std::floor(lo / paso) * paso;
    hi = std::ceil(hi / paso) * paso;
    return paso;
}

}  // namespace svg_detalle

inline bool guardarSVG(const Grafica& g, const std::string& ruta) {
    using namespace svg_detalle;
    const double W = 820, H = 520;
    const double L = 90, R = 30, T = 55, B = 75;  // márgenes
    const double pw = W - L - R, ph = H - T - B;

    // ---- rangos de datos ----
    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    for (const auto& s : g.series)
        for (size_t i = 0; i < s.x.size(); i++) {
            xmin = std::min(xmin, s.x[i]); xmax = std::max(xmax, s.x[i]);
            ymin = std::min(ymin, s.y[i]); ymax = std::max(ymax, s.y[i]);
        }
    if (g.tieneLineaH) { ymin = std::min(ymin, g.lineaH); ymax = std::max(ymax, g.lineaH); }
    if (g.yDesdeCero) ymin = 0;
    ymax = std::max(ymax, g.yMaxMinimo);
    if (xmax <= xmin) xmax = xmin + 1;

    double ylo = ymin, yhi = ymax;
    double ypaso = ejeBonito(ylo, yhi);
    double xlo, xhi, xpaso = 0;
    if (g.logX) { xlo = std::log2(xmin) - 0.3; xhi = std::log2(xmax) + 0.3; }
    else { xlo = xmin; xhi = xmax; xpaso = ejeBonito(xlo, xhi); }

    auto px = [&](double x) {
        double X = g.logX ? std::log2(x) : x;
        return L + (X - xlo) / (xhi - xlo) * pw;
    };
    auto py = [&](double y) { return T + ph - (y - ylo) / (yhi - ylo) * ph; };

    std::ostringstream o;
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W << "\" height=\"" << H
      << "\" viewBox=\"0 0 " << W << " " << H << "\" font-family=\"DejaVu Sans, Arial, sans-serif\">\n"
      << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n"
      << "<text x=\"" << W / 2 << "\" y=\"30\" text-anchor=\"middle\" font-size=\"18\" "
      << "font-weight=\"bold\">" << esc(g.titulo) << "</text>\n";

    // ---- grilla y ticks del eje Y ----
    for (double v = ylo; v <= yhi + ypaso * 1e-6; v += ypaso) {
        double y = py(v);
        o << "<line x1=\"" << L << "\" y1=\"" << y << "\" x2=\"" << L + pw << "\" y2=\"" << y
          << "\" stroke=\"#e0e0e0\"/>\n"
          << "<text x=\"" << L - 8 << "\" y=\"" << y + 4
          << "\" text-anchor=\"end\" font-size=\"12\">" << fmt(v) << "</text>\n";
    }
    // ---- grilla y ticks del eje X ----
    if (g.logX) {
        int k0 = (int)std::ceil(xlo), k1 = (int)std::floor(xhi);
        int paso = std::max(1, (k1 - k0 + 1 + 7) / 8);
        for (int k = k0; k <= k1; k += paso) {
            double x = px(std::pow(2.0, k));
            o << "<line x1=\"" << x << "\" y1=\"" << T << "\" x2=\"" << x << "\" y2=\"" << T + ph
              << "\" stroke=\"#e0e0e0\"/>\n"
              << "<text x=\"" << x << "\" y=\"" << T + ph + 20
              << "\" text-anchor=\"middle\" font-size=\"13\">" << potencia2(k) << "</text>\n";
        }
    } else {
        for (double v = xlo; v <= xhi + xpaso * 1e-6; v += xpaso) {
            double x = px(v);
            o << "<line x1=\"" << x << "\" y1=\"" << T << "\" x2=\"" << x << "\" y2=\"" << T + ph
              << "\" stroke=\"#e0e0e0\"/>\n"
              << "<text x=\"" << x << "\" y=\"" << T + ph + 20
              << "\" text-anchor=\"middle\" font-size=\"12\">" << fmt(v) << "</text>\n";
        }
    }
    // ---- ejes ----
    o << "<rect x=\"" << L << "\" y=\"" << T << "\" width=\"" << pw << "\" height=\"" << ph
      << "\" fill=\"none\" stroke=\"#333\"/>\n"
      << "<text x=\"" << L + pw / 2 << "\" y=\"" << H - 20
      << "\" text-anchor=\"middle\" font-size=\"14\">" << esc(g.etiquetaX) << "</text>\n"
      << "<text transform=\"translate(22," << T + ph / 2 << ") rotate(-90)\" "
      << "text-anchor=\"middle\" font-size=\"14\">" << esc(g.etiquetaY) << "</text>\n";

    // ---- línea horizontal de referencia ----
    if (g.tieneLineaH) {
        double y = py(g.lineaH);
        o << "<line x1=\"" << L << "\" y1=\"" << y << "\" x2=\"" << L + pw << "\" y2=\"" << y
          << "\" stroke=\"gray\" stroke-width=\"1.5\" stroke-dasharray=\"6 4\"/>\n";
    }

    // ---- series ----
    for (const auto& s : g.series) {
        o << "<polyline fill=\"none\" stroke=\"" << s.color << "\" stroke-width=\"2.2\"";
        if (s.punteada) o << " stroke-dasharray=\"7 4\"";
        o << " points=\"";
        for (size_t i = 0; i < s.x.size(); i++) o << px(s.x[i]) << "," << py(s.y[i]) << " ";
        o << "\"/>\n";
        for (size_t i = 0; i < s.x.size(); i++) {
            double x = px(s.x[i]), y = py(s.y[i]);
            if (s.cuadrado)
                o << "<rect x=\"" << x - 3.5 << "\" y=\"" << y - 3.5
                  << "\" width=\"7\" height=\"7\" fill=\"" << s.color << "\"/>\n";
            else
                o << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"4\" fill=\""
                  << s.color << "\"/>\n";
        }
    }

    // ---- leyenda ----
    std::vector<std::pair<std::string, const Serie*>> entradas;
    size_t maxLen = 0;
    for (const auto& s : g.series) { entradas.push_back({s.nombre, &s}); maxLen = std::max(maxLen, s.nombre.size()); }
    if (g.tieneLineaH) maxLen = std::max(maxLen, g.etiquetaLineaH.size());
    double lx = L + 12, ly = T + 12;
    double lw = 50 + 7.0 * (double)maxLen, lh = 12 + 22.0 * (double)(entradas.size() + (g.tieneLineaH ? 1 : 0));
    o << "<rect x=\"" << lx << "\" y=\"" << ly << "\" width=\"" << lw << "\" height=\"" << lh
      << "\" fill=\"white\" fill-opacity=\"0.9\" stroke=\"#bbb\" rx=\"4\"/>\n";
    double yy = ly + 20;
    for (auto& e : entradas) {
        const Serie& s = *e.second;
        o << "<line x1=\"" << lx + 8 << "\" y1=\"" << yy - 4 << "\" x2=\"" << lx + 34 << "\" y2=\"" << yy - 4
          << "\" stroke=\"" << s.color << "\" stroke-width=\"2.2\""
          << (s.punteada ? " stroke-dasharray=\"7 4\"" : "") << "/>\n"
          << "<text x=\"" << lx + 42 << "\" y=\"" << yy << "\" font-size=\"13\">" << esc(e.first) << "</text>\n";
        yy += 22;
    }
    if (g.tieneLineaH) {
        o << "<line x1=\"" << lx + 8 << "\" y1=\"" << yy - 4 << "\" x2=\"" << lx + 34 << "\" y2=\"" << yy - 4
          << "\" stroke=\"gray\" stroke-width=\"1.5\" stroke-dasharray=\"6 4\"/>\n"
          << "<text x=\"" << lx + 42 << "\" y=\"" << yy << "\" font-size=\"13\">" << esc(g.etiquetaLineaH) << "</text>\n";
    }
    o << "</svg>\n";

    std::ofstream f(ruta);
    if (!f) return false;
    f << o.str();
    return true;
}
