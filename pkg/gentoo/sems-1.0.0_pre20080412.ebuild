# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-libs/spandsp/spandsp-0.0.3_pre26.ebuild,v 1.1 2006/11/27 21:05:46 drizzt Exp $

inherit versionator

IUSE=""

DESCRIPTION="SEMS is a free, high performance, extensible media server for SIP (RFC3261) based VoIP  services."
HOMEPAGE="http://iptel.org/sems/"

S="${WORKDIR}/${PN}-$(get_version_component_range 1-3)"
SRC_URI="http://ftp.iptel.org/pub/sems/testing/${P/_/}.tar.gz"

SLOT="0"
LICENSE="GPL-2"
KEYWORDS="~amd64 ~ppc ~x86"

DEPEND=">=media-libs/spandsp-0.0.4_pre18
	>=media-libs/speex-1.1.7
	>=dev-lang/python-2.4.4-r4"

src_compile () {
    if [ -x ./configure ]; then
        econf
    fi
    if [ -f Makefile ] || [ -f GNUmakefile ] || [ -f makefile ]; then
        emake PREFIX=/usr RELEASE=${PVR} || die "emake failed"
    fi
}

src_install () {
	emake PREFIX=/usr cfg-target="/etc/sems/" DESTDIR="${D}" install || die
	dodoc README
}
