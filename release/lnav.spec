# vim: set ts=4 sw=4 et:
#
# spec file for package lnav
#
# Copyright (c) 2023 SUSE LLC
# Copyright (c) 2010-2013 Pascal Bleser <pascal.bleser@opensuse.org>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

Name:           lnav
Version:        0.11.2
Release:        0
Summary:        Logfile Navigator
License:        BSD-2-Clause
Group:          System/Monitoring
URL:            https://lnav.org
# Git-Clone:    https://github.com/tstack/lnav.git
# Source:       https://github.com/tstack/%{name}/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
Source:         /github/home/rpmbuild/SOURCES/%{name}-%{version}.tar.gz
# Source1:      lnav.desktop
BuildRequires:  gcc-toolset-12
BuildRequires:  gcc-toolset-12-annobin-plugin-gcc
BuildRequires:  gcc-toolset-12-annobin-annocheck
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libarchive-devel
BuildRequires:  libcurl-devel
BuildRequires:  ncurses-devel
# Only needed for the tests to run
BuildRequires:  openssh
BuildRequires:  bzip2-devel
BuildRequires:  pcre2-devel
BuildRequires:  readline-devel
BuildRequires:  zlib-devel
%if 0%{?suse_version}
BuildRequires:  sqlite3-devel >= 3.9.0
%else
BuildRequires:  sqlite-devel >= 3.9.0
%endif
%if 0%{?suse_version} > 0
BuildRequires:  update-desktop-files
%endif

%description
The Logfile Navigator, lnav for short, is a curses-based tool for viewing and
analyzing log files. The value added by lnav over text viewers or editors is
that it takes advantage of any semantic information that can be gleaned from
the log file, such as timestamps and log levels. Using this extra semantic
information, lnav can do things like interleaving messages from different
files, generate histograms of messages over time, and provide hotkeys for
navigating through the file. These features are meant to allow the user to
quickly and efficiently focus on problems.

%prep
%autosetup -p1

%build
source /opt/rh/gcc-toolset-12/enable
autoreconf -fiv
%configure \
     --disable-silent-rules \
     --disable-static \
     --with-ncurses \
     --with-readline || cat config.log

%make_build

%install
%make_install

%if %{defined suse_version}
install -D -m0644 "%{SOURCE1}" "%{buildroot}%{_datadir}/applications/%{name}.desktop"
%suse_update_desktop_file -r "%{name}" System Monitor
%endif

%files
%license LICENSE
%doc AUTHORS NEWS.md README
%{_bindir}/lnav
%{_mandir}/man1/lnav.1%{?ext_man}
%if %{defined suse_version}
%{_datadir}/applications/%{name}.desktop
%endif

%changelog
