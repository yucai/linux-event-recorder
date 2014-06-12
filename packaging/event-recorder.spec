Name: event-recorder
Version: 0.6.0
Release: 1
Summary: event-recorder
Group: System/Libraries
License: GPLv2
Source: %{name}-%{version}.tar.gz


%description
Record and playback user operation, including touch, mouse, keyboard etc.

%prep
%setup -q

%build
make

%install
make install DESTDIR=%{buildroot} 

%post
%postun

%clean 
rm -rf %{buildroot}
%files
/usr/bin/ev_playback
/usr/bin/ev_record
/usr/bin/process_event
%doc

%changelog

