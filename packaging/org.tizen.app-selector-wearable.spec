Name:       org.tizen.app-selector-wearable
Summary:    Application selector
Version:    0.1.57
Release:    1
Group:      TO_BE/FILLED_IN
License:    Flora-1.1
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  gettext-tools
BuildRequires:  edje-tools
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(eina)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-file)
BuildRequires:  pkgconfig(edje)
BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(xcomposite)
BuildRequires:  pkgconfig(xext)
BuildRequires:  pkgconfig(efl-assist)
BuildRequires:  pkgconfig(pkgmgr-info)
BuildRequires:  hash-signer

%description
SLP application selector
 
 
%prep
%setup -q


%build
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif
export CFLAGS="$CFLAGS -Wall -Werror -Wno-unused-function"
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DAPP_SELECTOR_VERSION="%{version}"
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
#Signing
%define tizen_sign_base /usr/apps/%{name}
%define tizen_sign 1
%define tizen_author_sign 1
%define tizen_dist_sign 1
%define tizen_sign_level platform
mkdir -p %{buildroot}/usr/share/license
install LICENSE %{buildroot}/usr/share/license/%{name}

%post
/usr/bin/signing-client/hash-signer-client.sh -a -d -p platform /usr/apps/%{name}
 
%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
/usr/apps/%{name}/bin/*
/usr/apps/%{name}/res/edje/*
/usr/share/packages/%{name}.xml
/etc/smack/accesses.d/%{name}.efl
#Signing
/usr/apps/%{name}/author-signature.xml
/usr/apps/%{name}/signature1.xml
/usr/share/license/%{name}
