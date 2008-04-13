# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-libs/spandsp/spandsp-0.0.3_pre26.ebuild,v 1.1 2006/11/27 21:05:46 drizzt Exp $

inherit versionator

IUSE=""

DESCRIPTION="SpanDSP is a library of DSP functions for telephony."
HOMEPAGE="http://www.soft-switch.org/"

S="${WORKDIR}/${PN}-$(get_version_component_range 1-3)"
SRC_URI="http://www.soft-switch.org/downloads/spandsp/${P/_/}.tgz"

SLOT="0"
LICENSE="GPL-2"
KEYWORDS="~amd64 ~ppc ~x86"

DEPEND=">=media-libs/audiofile-0.2.6-r1
	>=media-libs/tiff-3.5.7-r1"

src_install () {
	echo $S
#	einstall || die
	emake DESTDIR="${D}" install || die
	dodoc AUTHORS INSTALL NEWS README
}
