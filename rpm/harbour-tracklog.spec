Name:       harbour-tracklog
Summary:    GPS logger with on-device history and background daemon
Version:    0.1
Release:    5
License:    MIT
URL:        https://github.com/hensip/harbour-tracklog
Source0:    %{name}-%{version}.tar.bz2

Requires:   sailfishsilica-qt5 >= 0.10.9

BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Positioning)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  desktop-file-utils

%description
TrackLog records GPS positions on the device and keeps a local history that can
be browsed on a map with a date filter. A background daemon keeps recording when
the application is closed, and optionally uploads the points to PhoneTrack.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
  %{buildroot}%{_datadir}/applications/*.desktop

install -D -m 644 packaging/tracklogd.service \
  %{buildroot}%{_prefix}/lib/systemd/user/tracklogd.service
install -D -m 644 packaging/TrackLog.permission \
  %{buildroot}/etc/sailjail/permissions/TrackLog.permission

%post
if [ -d /home/defaultuser ]; then
  mkdir -p /home/defaultuser/.config/systemd/user/default.target.wants
  ln -sf /usr/lib/systemd/user/tracklogd.service \
    /home/defaultuser/.config/systemd/user/default.target.wants/tracklogd.service
  chown -R defaultuser:defaultuser /home/defaultuser/.config/systemd/user
fi

%preun
if [ -d /home/defaultuser ]; then
  rm -f /home/defaultuser/.config/systemd/user/default.target.wants/tracklogd.service
fi

%files
%defattr(-,root,root,-)
%{_bindir}/harbour-tracklog
%{_bindir}/tracklogd
%{_datadir}/harbour-tracklog
%{_datadir}/applications/harbour-tracklog.desktop
%{_datadir}/icons/hicolor/*/apps/harbour-tracklog.png
%{_prefix}/lib/systemd/user/tracklogd.service
/etc/sailjail/permissions/TrackLog.permission
