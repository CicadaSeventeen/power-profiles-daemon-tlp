#
# spec file for package power-profiles-daemon
#
# Copyright (c) 2025 SUSE LLC
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#


Name:           ppd-tlp
Version:        0.30
Release:        0
Summary:        A fork of power-profiles-daemon to use tlp as backend.
License:        GPL-3.0-or-later
URL:            https://github.com/CicadaSeventeen/power-profiles-daemon-tlp
Source:         %{name}-%{version}.tar.bz2
# PATCH-FEATURE-OPENSUSE hold-profile-hardening.patch boo#1189900 -- Hardening of HoldProfile D-Bus method
Patch0:         hold-profile-hardening.patch
BuildRequires:  c_compiler
BuildRequires:  cmake
BuildRequires:  gtk-doc
BuildRequires:  meson >= 0.59.0
BuildRequires:  pkgconfig
BuildRequires:  python3-argparse-manpage
BuildRequires:  python3-dbusmock
BuildRequires:  python3-shtab
BuildRequires:  pkgconfig(bash-completion)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gudev-1.0) >= 234
BuildRequires:  pkgconfig(polkit-gobject-1) >= 0.91
BuildRequires:  pkgconfig(systemd)
BuildRequires:  pkgconfig(udev)
BuildRequires:  pkgconfig(umockdev-1.0)
BuildRequires:  pkgconfig(upower-glib)
Requires:       polkit
Requires:       tlp
Requires:       python3-gobject
Conflicts:      ppd-service
Conflicts:      power-profiles-daemon


%description
A fork of power-profiles-daemon to use tlp as backend.

%package doc
Summary:        Documentation for power-profiles-daemon
BuildArch:      noarch

%description doc
This package provides documentation for %{name}.

%prep
%setup -q -n %{name}-%{version}

%build
%meson \
	-Dsystemdsystemunitdir=%{_unitdir} \
	-Dgtk_doc=true \
	-Dpylint=disabled \
	-Dzshcomp=%{_datadir}/zsh/site-functions/ \
	%{nil}
%meson_build

%install
%meson_install
#rm %{buildroot}/%{_datadir}/bash-completion/completions/powerprofilesctl
#rm %{buildroot}/%{_datadir}/zsh/site-functions/_powerprofilesctl
#mv %{buildroot}/%{_unitdir}/power-profiles-daemon.service %{buildroot}/%{_unitdir}/ppd-tlp.service
%{python3_fix_shebang}

%check
%meson_test

%pre
%service_add_pre ppd-tlp.service

%post
%service_add_post ppd-tlp.service

%preun
%service_del_preun ppd-tlp.service

%postun
%service_del_postun ppd-tlp.service

%files
%license COPYING
%doc README.md
%{_bindir}/powerprofilesctl
%{_libexecdir}/power-profiles-daemon
%{_unitdir}/ppd-tlp.service
%{_datadir}/dbus-1/system.d/net.hadess.PowerProfiles.conf
%{_datadir}/dbus-1/system.d/org.freedesktop.UPower.PowerProfiles.conf
%{_datadir}/dbus-1/system-services/net.hadess.PowerProfiles.service
%{_datadir}/dbus-1/system-services/org.freedesktop.UPower.PowerProfiles.service
%{_datadir}/polkit-1/actions/power-profiles-daemon.policy
%{_mandir}/man1/powerprofilesctl.1%{?ext_man}
%{_datadir}/bash-completion/completions/powerprofilesctl
%{_datadir}/zsh/site-functions/_powerprofilesctl
%ghost %attr(0755,root,root) %dir %{_localstatedir}/lib/power-profiles-daemon

%files doc
%dir %{_datadir}/gtk-doc/
%dir %{_datadir}/gtk-doc/html/
%{_datadir}/gtk-doc/html/power-profiles-daemon/

%changelog
