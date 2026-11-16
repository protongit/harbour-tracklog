# Changelog

## 0.2 (2026-11-16)

- Histórico: estadísticas (distancia, velocidad media/máxima, desnivel), gráfica
  de velocidad, recorridos detectados y renombrables, y página de puntos del día.
- Mapa: traza coloreada por velocidad, corte en huecos, marcadores periódicos,
  fuentes OSM/OpenTopoMap/CyclOSM y descarga de zona con caché por fuente.
- Captura: intervalo adaptativo al estar parado, fallback de 25 m sin precisión,
  pausa por batería baja y nivel de batería en cada punto.
- Fiabilidad: esquema versionado (`user_version`), precisión desconocida como
  `NULL`, checkpoint WAL al cerrar y `tracklogd --selftest`.
- Datos: retención configurable, copia de seguridad de la base y exportación e
  importación de GPX (con segmentos).
- Subida: botón "Probar conexión", API D-Bus ampliada (`TestUpload`,
  `GetPoints`, `ExportGpx`, `ImportGpx`, `BackupDatabase`).
- Geocerca con notificación de entrada/salida.
- Cubierta con antigüedad del último punto.
- CI (GitHub Actions) y metadatos Chum.

## 0.1 (2026-11-15)

- Primera versión: demonio `tracklogd` con SQLite y D-Bus, servicio systemd con
  autostart, app de control, histórico con filtro de fechas y mapa OSM,
  exportación GPX y subida a PhoneTrack.
