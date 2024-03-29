%define ver	0.6.6
%define release	1
%define	prefix	/usr

Summary:	A full featured IRC-bouncer.
Name:		miau
Version:	%{ver}
Release:	%{release}
Copyright:	GPL
Source:		http://miau.sourceforge.net/files.html
URL:		http://miau.sourceforge.net/
Group:		Applications/Net
BuildRoot:	%{_tmppath}/%{name}-root

%description 
miau is a smart and versatile irc-bouncing tool for unix. The difference between
miau and other bouncers is that miau will go on irc as soon as it's launched,
guarding or attempting to get your nick. Control over the session can be taken
as with other bouncers, by simply connecting to miau (and providing a password)
like you would connect to a normal irc-server. On disconnect, miau is able to
stay in the channels and to reintroduce them to your client on your next
connect. Other handy features are message-logging, flood-protection,
dcc-bouncing, etc. (obsolete description)

%prep
%setup
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
%build
make

%install
rm -rf $RPM_BUILD_ROOT
make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%{prefix}

%changelog
* Fri Nov  1 2002 Tommi Saviranta <wnd@iki.fi>
- Branched muh to miau
- A lot done ever since

* Thu Mar 21 2002 Sebastian Kienzl <zap@riot.org>
- Spec file cleanup

* Fri Dec 29 2000 Bastian Friedrich <bastian@bastian-friedrich.de>
- Initial RPM release.
