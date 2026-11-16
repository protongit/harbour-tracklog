# TrackLog

Registrador GPS para Sailfish OS con almacenamiento local, demonio en segundo
plano e histórico sobre mapa.

Repositorio: <https://github.com/protongit/harbour-tracklog>

```sh
git clone https://github.com/protongit/harbour-tracklog.git
```

Versión instalada y probada: **0.2** en Jolla Phone 2026 (Sailfish OS 5.2.0.22).
Licencia MIT. Ver `CHANGELOG.md` para el historial de cambios.

## Qué hace

- **Graba en local**: cada punto se guarda en SQLite antes de nada. Nada se
  pierde si no hay red.
- **Demonio independiente**: `tracklogd` corre como servicio de usuario y sigue
  grabando con la app cerrada, la pantalla bloqueada o tras reiniciar.
- **Control desde la app**: iniciar/parar la grabación y arrancar/parar el
  servicio, con estado en vivo (GPS, precisión, puntos, pendientes, errores).
- **Histórico**: filtro por periodo (hoy / 7 / 30 días, rango con fechas o
  tocando un día), traza coloreada por velocidad sobre mapa OSM con caché,
  estadísticas (distancia, velocidad media/máxima, desnivel), gráfica de
  velocidad, recorridos detectados y renombrables, y lista de puntos del día.
  Exporta e **importa GPX** en `~/Documents/TrackLog/`.
- **Batería**: intervalo adaptativo (se amplía al estar parado) y pausa
  automática por debajo de un nivel de batería configurable. Registra el nivel
  de batería en cada punto.
- **Subida opcional** a PhoneTrack (o URL estilo Traccar), con cola persistente,
  reintentos con backoff y sin borrar nunca los puntos locales.

## Arquitectura

```
harbour-tracklog (UI, sailjailed)
        │  D-Bus: org.hensip.tracklog
        ▼
tracklogd (systemd --user, fuera de sailjail)
        ├── QGeoPositionInfoSource (geoclue)
        ├── SQLite  ~/.local/share/org.hensip/tracklog/tracklog.db
        └── QNetworkAccessManager (subida)
```

- Servicio: `tracklogd.service` (`Type=dbus`, `BusName=org.hensip.tracklog`).
- Activación D-Bus vía systemd: `/usr/share/dbus-1/services/org.hensip.tracklog.service`
  con `SystemdService=tracklogd.service`.
- Permiso de sailjail: `/etc/sailjail/permissions/TrackLog.permission`
  (la app necesita hablar con el nombre D-Bus de su demonio).

### API D-Bus

`org.hensip.tracklog` · `/org/hensip/tracklog` · `org.hensip.tracklog.Control`

- `Start()`, `Stop()`, `Quit()`
- `GetStatus() → a{sv}`, `GetConfig() → a{sv}`, `SetConfig(a{sv}) → b`
- Señales: `StatusChanged(a{sv})`, `PointAdded(x d d x)`

## Datos

| Ruta | Contenido |
|---|---|
| `~/.local/share/org.hensip/tracklog/tracklog.db` | puntos (WAL), estado de subida |
| `~/.local/share/org.hensip/tracklog/settings.ini` | configuración |
| `~/.local/share/org.hensip/tracklog/tiles/<fuente>/` | caché de tiles (OSM, OpenTopoMap, CyclOSM) |
| `~/.local/share/org.hensip/tracklog/trips.ini` | nombres personalizados de recorridos |
| `~/.local/share/org.hensip/tracklog/tracklogd.log` | log del demonio (rota a 512 KB) |
| `~/.local/share/org.hensip/tracklog/app.log` | log de la app (rota a 256 KB) |
| `~/Documents/TrackLog/*.gpx` | exportaciones e importaciones GPX |
| `~/Documents/TrackLog/*.db` | copias de seguridad de la base |

## Compilar

```sh
cd /home/hensip/Proyectos/SailfishOS/harbour-tracklog
sg docker -c 'DOCKER_HOST=unix:///var/run/docker.sock ~/SailfishOS/bin/sfdk \
  -c target=SailfishOS-5.1.0.11-aarch64 build'
```

El RPM queda en `RPMS/harbour-tracklog-<version>-<release>.aarch64.rpm`.

> El árbol debe estar commiteado antes de compilar: `sfdk` cachea el tarball de
> fuentes cuando no hay git y reutiliza código viejo. Sube `Release` en el
> `.spec` para cada instalación de prueba.

## Instalar

```sh
scp RPMS/harbour-tracklog-*.aarch64.rpm defaultuser@<telefono>:/home/defaultuser/
ssh defaultuser@<telefono>
devel-su pkcon install-local -y /home/defaultuser/harbour-tracklog-*.aarch64.rpm
```

El `%post` crea el enlace de autostart en
`~/.config/systemd/user/default.target.wants/`.

## Configuración (Ajustes)

- **URL del servidor**: para PhoneTrack, `https://servidor/apps/phonetrack/logGet/<token>/`
  (el ID se añade detrás). Con "URL estilo Traccar" se usa `?id=<id>`.
- **ID del dispositivo**: nombre con el que aparecerá en PhoneTrack.
- **Intervalo**: 30 s / 60 s / 2 min / 5 min.
- **Precisión máxima (m)**: descarta puntos con peor precisión.
- **Distancia mínima (m)**: separación mínima entre puntos (además de la
  precisión actual).
- **Subir puntos**: activa la cola de subida.
- **Pausar por debajo de batería (%)**: 0 desactiva la pausa.
- **Conservar histórico (días)**: 0 conserva todo; los puntos ya subidos se
  podan a diario.
- **Fuente de tiles**: OpenStreetMap, OpenTopoMap o CyclOSM.
- **Geocerca**: aviso al entrar o salir de un radio (nombre, latitud, longitud,
  radio).
- **Probar conexión**, **copia de seguridad** y gestión de la caché de mapas.
- **Grabar al iniciar**: arranca la grabación al iniciar sesión.

La captura descarta puntos con precisión peor de 120 m y, en movimiento, exige
una distancia mayor que la precisión actual para evitar ruido estando parado.

## Servicio

```sh
systemctl --user status tracklogd
systemctl --user start tracklogd     # o desde la app
systemctl --user stop tracklogd
journalctl --user -u tracklogd       # requiere grupo systemd-journal
```

Tras `Quit` (botón "Parar servicio"), la siguiente llamada D-Bus lo vuelve a
arrancar a través de systemd.

## Diagnóstico

- `tracklogd.log`: arranque, fixes, puntos grabados, resultado de cada subida y
  reintentos.
- `app.log`: errores QML y de arranque de la app.
- Comprobación QML sin interfaz: `harbour-tracklog --check`.
- El modo `--open-history` abre el histórico automáticamente y sale a los 8 s
  (solo para depurar).

## Mapa

Tiles de OpenStreetMap descargados por la app y cacheados en disco con
`User-Agent` propio. Las zonas ya visitadas se ven sin conexión. Atribución
visible en el mapa (`© OpenStreetMap`).

## Estado

Implementado y probado en Jolla Phone 2026 (Sailfish OS 5.2.0.22):

- [x] Demonio con GPS headless, SQLite y D-Bus
- [x] Servicio systemd con autostart y activación D-Bus
- [x] Permiso sailjail para el D-Bus propio
- [x] App de control (estado, grabación, servicio, ajustes)
- [x] Histórico con filtro de fechas, lista de días y mapa con caché
- [x] Exportar e importar GPX
- [x] Pinch-zoom y filtros de captura configurables
- [x] Subida a PhoneTrack con cola y reintentos
- [x] Estadísticas, gráfica de velocidad y recorridos renombrables
- [x] Intervalo adaptativo y pausa por batería baja
- [x] Mapa con traza coloreada, caché por fuente y descarga de zona
- [x] Geocerca con notificación
- [x] Retención configurable y copia de seguridad de la base
- [x] Versionado de esquema (SQLite `user_version`)
- [x] CI (GitHub Actions) y metadatos Chum

Pendiente / ideas:

- [ ] Traducciones (el target del SDK no trae `lupdate/lrelease`)
- [ ] Cifrado del histórico con SQLCipher + Sailfish Secrets
- [ ] Empaquetado real en Chum/Harbour
- [ ] Perfiles de batería predefinidos y medida de consumo en campo

## Licencia

MIT. Ver `LICENSE`.
