[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/panel-plugins/xfce4-wavelan-plugin/-/blob/master/COPYING)

# xfce4-wavelan-plugin

Xfce4-wavelan-plugin is used to display stats from a wireless LAN interface.

Xfce4-wavelan-plugin displays the following information about a WaveLAN device:

* Signal state (tells if a carrier signal was detected)
* Signal quality (current quality of the carrier signal)
  * Note that the latter is in % on Linux and in dBm on BSDs. Hence, on BSDs, the progressbar may be never full, as dBm is not easily comparable to a maximum.
* Network name (current SSID of the WaveLAN network)

At the time of this writing NetBSD, OpenBSD, FreeBSD and Linux are supported.

----

### Homepage

[Xfce4-wavelan-plugin documentation](https://docs.xfce.org/panel-plugins/xfce4-wavelan-plugin)

### Changelog

See [NEWS](https://gitlab.xfce.org/panel-plugins/xfce4-wavelan-plugin/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Xfce4-wavelan-plugin source code](https://gitlab.xfce.org/panel-plugins/xfce4-wavelan-plugin)

### Download a Release Tarball

[Xfce4-wavelan-plugin archive](https://archive.xfce.org/src/panel-plugins/xfce4-wavelan-plugin)
    or
[Xfce4-wavelan-plugin tags](https://gitlab.xfce.org/panel-plugins/xfce4-wavelan-plugin/-/tags)

### Installation

From source code repository: 

    % cd xfce4-wavelan-plugin
    % ./autogen.sh
    % make
    % make install

From release tarball:

    % tar xf xfce4-wavelan-plugin-<version>.tar.bz2
    % cd xfce4-wavelan-plugin-<version>
    % ./configure
    % make
    % make install

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/panel-plugins/xfce4-wavelan-plugin/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

