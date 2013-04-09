#
# spec file for package u-boot-chromebook
#
# Copyright (c) 2012 SUSE LINUX Products GmbH, Nuernberg, Germany.
# Copyright (c) 2010 Texas Instruments Inc by Nishanth Menon
# Copyright (c) 2007-2010 by Silvan Calarco <silvan.calarco@mambasoft.it>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


%define x_loader 0

Name:           u-boot-chromebook
Version:        2012.12.chromebook
Release:        1.20.1
Summary:        The u-boot firmware for the chromebook arm platform
License:        GPL-2.0
Group:          System/Boot
Url:            http://www.denx.de/wiki/U-Boot
Source:         u-boot-%{version}.tar.bz2
Source1:        openSUSE_panda.txt
Source300:      rpmlintrc
Patch1:         fix-chromebook.patch
Patch2:         patch-chromebook.patch
Patch3:         fix-usb.patch
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Provides:       u-boot-loader
Conflicts:      otherproviders(u-boot-loader)
%if %x_loader == 1
Obsoletes:      x-loader-chromebook
Provides:       x-loader-chromebook
%endif
BuildRequires:  dtc
ExclusiveArch:  %arm

%description
Das U-Boot (or just "U-Boot" for short) is Open Source Firmware for Embedded PowerPC, ARM, MIPS and x86 processors.
This package contains the firmware for the chromebook arm platform.

%package doc
Summary:        Documentation for the u-boot Firmware
Group:          Documentation/Other

%description doc
Das U-Boot (or just "U-Boot" for short) is Open Source Firmware for Embedded PowerPC, ARM, MIPS and x86 processors.
This package contains documentation for u-boot firmware

%prep
%setup -q -n u-boot-%{version}
# Any custom patches to be applied on top of mainline u-boot
%patch1 -p1
%patch2 -p1
%patch3 -p1
#%patch4 -p1

%build
make %{?jobs:-j %jobs} CFLAGS="$RPM_OPT_FLAGS" chromeos_daisy_config
# temporary disable of --build-id
#make CFLAGS="$RPM_OPT_FLAGS" USE_PRIVATE_LIBGG=yes
make %{?jobs:-j %jobs} USE_PRIVATE_LIBGG=yes
./tools/mkimage -A arm -O linux -T kernel -C none -a 0x47e00000 -e 0x47e00000 -n uboot -d u-boot-dtb.bin u-boot.img

%install
install -D -m 0644 u-boot.bin %{buildroot}/boot/u-boot.bin
install -D -m 0644 u-boot-dtb.bin %{buildroot}/boot/u-boot-dtb.bin
install -D -m 0644 u-boot.img %{buildroot}/boot/u-boot.img
%if %x_loader == 1
install -D -m 0755 MLO %{buildroot}/boot/MLO
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/boot/u-boot.bin
/boot/u-boot-dtb.bin
/boot/u-boot.img
%if %x_loader == 1
/boot/MLO
%endif
%doc COPYING CREDITS README

%files doc
%defattr(-,root,root)
# Generic documents
%doc doc/README.JFFS2 doc/README.JFFS2_NAND doc/README.commands
%doc doc/README.autoboot doc/README.commands doc/README.console doc/README.dns
%doc doc/README.hwconfig doc/README.nand doc/README.NetConsole doc/README.serial_multi
%doc doc/README.SNTP doc/README.standalone doc/README.update doc/README.usb
%doc doc/README.video doc/README.VLAN doc/README.silent doc/README.POST doc/README.Modem
# Copy some useful kermit scripts as well
%doc tools/scripts/dot.kermrc tools/scripts/flash_param tools/scripts/send_cmd tools/scripts/send_image
# Now any h/w dependent Documentation
%doc doc/README.ARM-SoC doc/README.ARM-memory-map 

%changelog
* Tue Jul 24 2012 dmueller@suse.com
- remove u-boot-omap3beagle
* Mon Jul 23 2012 agraf@suse.com
- bump to 2012.04.01
  - fixes bug in cmdline parsing
* Mon Jul 23 2012 agraf@suse.com
- add calxeda highbank support
* Thu Jul 12 2012 agraf@suse.com
- autoload boot.scr on beagle, so we can boot again
* Thu Jul 12 2012 agraf@suse.com
- update to upstream u-boot 2012.04
  - > gets rid of linaro fork, only mainline now
  - > gets us omap3 MLO support, no more need for x-loader
  - > potentially fixes voltage issues on omap4
* Thu Jun 14 2012 adrian@suse.de
- add SUSE style conflicts to avoid installation of multiple
  boot loaders
* Tue Apr 17 2012 joop.boonen@opensuse.org
- Included u-boot.spec.in and gen_spec.sh in the spec file
* Mon Feb  6 2012 agraf@suse.com
- use ext2 on panda
* Tue Dec 20 2011 agraf@suse.com
- use ttyO2 as default console= on OMAP boards
* Mon Dec 19 2011 agraf@suse.com
- add u8500_href and origen configs
* Fri Dec 16 2011 agraf@suse.com
- fix lint failures
* Fri Dec 16 2011 agraf@suse.com
- don't install map
* Fri Dec 16 2011 agraf@suse.com
- generalize spec file to be able to build for more boards
- add beagle board spec file
- remove boot.scr
* Fri Dec 16 2011 agraf@suse.com
- rename to u-boot-omap4panda
* Tue Dec 13 2011 dkukawka@suse.de
- new package based on u-boot-omap4panda but use linaro u-boot git
  repo (http://git.linaro.org/git/boot/u-boot-linaro-stable.git)
  instead of mainline u-boot. This package also contains the MLO
  (this package obsoletes the x-loader package)
* Tue Nov 29 2011 joop.boonen@opensuse.org
- COPYING CREDITS README are now in the standard package
* Thu Nov 24 2011 joop.boonen@opensuse.org
- Corrected the links
* Tue Nov 22 2011 joop.boonen@opensuse.org
- Build without u-boot tools as we have a u-boot-tools packages
* Sun Nov 20 2011 joop.boonen@opensuse.org
- Cleaned the spec file up the spec file
- The name is the same as the package name
* Sun Nov 13 2011 joop.boonen@opensuse.org
- Build u-boot according to http://elinux.org/Panda_How_to_MLO_&_u-boot
- Using .txt config file instead of .scr it's gerated via mkimage
* Wed Nov  9 2011 joop.boonen@opensuse.org
- Used scr file based on http://elinux.org definition
- Build u-boot 20111109
- Used the Meego panda u-boot as a base
* Fri Feb 18 2011 raghuveer.murthy@ti.com> 
- 2010.09-MeeGo
- Fix for u-boot fails to compile on armv7hl, BMC#13140
* Thu Nov 18 2010 peter.j.zhu@intel.com> 
- 2010.09-MeeGo
- Don't build against i586, BMC#10159
* Sun Oct 10 2010 nm@ti.com> 
- 2010.09-MeeGo
- Add Das u-boot package - FEA#9723
* Sun Oct 10 2010 nm@ti.com> 
- 2010.09.rc1-MeeGo
- Added option to enable boot.scr generation and copy
* Mon Oct  4 2010 nm@ti.com> 
- 2010.09.rc1-MeeGo
- Update to 2010.09
* Tue Sep 14 2010 nm@ti.com> 
- 2010.09.rc1-MeeGo
- Update to 2010.09.rc1
- MeeGo customization
- Enabled PandaBoard, Beagleboard build
* Wed Mar 31 2010 silvan.calarco@mambasoft.it> 
- 2009.11.1-1mamba
- update to 2009.11.1
