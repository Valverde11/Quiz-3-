| Algoritmo | Métrica | Constante `c` | R² |
|---|---|---|---|
| Búsqueda binaria | Comparaciones | 0.966 | 0.99872 |
| Búsqueda binaria | Tiempo (n ≤ 2¹⁷) | 5.76 ns | 0.98294 |
| Mergesort | Tiempo | 3.41e-06 ms | 0.99999 |
| Mergesort | Comparaciones | 0.943 | 0.99999 |

| Modelo (Mergesort) | R² (comparaciones) | R² (tiempo) |
|---|---|---|
| n | 0.99851 | 0.99893 |
| n·log₂n | 0.99999 | 0.99999 |
| n² | 0.91874 | 0.91508 |
