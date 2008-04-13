# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-libs/spandsp/spandsp-0.0.3_pre26.ebuild,v 1.1 2006/11/27 21:05:46 drizzt Exp $

inherit versionator eutils

IUSE="spandsp speex python libsamplerate lame mpg123 sip examples"

DESCRIPTION="SEMS is a free, high performance, extensible media server for SIP (RFC3261) based VoIP  services."
HOMEPAGE="http://iptel.org/sems/"

S="${WORKDIR}/${PN}-$(get_version_component_range 1-3)"
SRC_URI="http://ftp.iptel.org/pub/sems/testing/${P/_/}.tar.gz"

SLOT="0"
LICENSE="GPL-2"
KEYWORDS="~amd64 ~ppc ~x86"

DEPEND="spandsp? >=media-libs/spandsp-0.0.4_pre18
	speex? ( >=media-libs/speex-1.1.7 )
	python? ( >=dev-lang/python-2.4.4-r4 )
	lame? ( >= media-sound/lame-3.97-r1 ) 
	mpg123?	( >= media-sound/mpg123-1.3.1 )
	sip? ( >= dev-python/sip-4.2.1 )
	libsamplerate? ( >= media-libs/libsamplerate-0.1.2 )"

src_unpack () {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}/${P}-exclude_modules_colon.patch"
}

src_compile () {
	local myexclude=""
	use examples || myexclude="examples"
	use speex || myexclude="${myexclude};speex"
	use python || myexclude="${myexclude};ivr;mailbox;pin_collect;conf_auth"
	use lame || myexclude="${myexclude};mp3"
	use sip || myexclude="${myexclude};py_sems"

	local myconf=""
	use spandsp && myconf="${myconf} USE_SPANDSP=yes"
	use spandsp || myconf="${myconf} WITH_MPG123DECODER=no"
	use libsamplerate || myconf="${myconf} USE_LIBSAMPLERATE=yes"

	if [ "${myexclude}" != "" ]; then 
		myconf="${myconf} exclude_modules=${myexclude}"
	fi

	echo "runnning 'emake PREFIX=/usr RELEASE=${PVR} ${myconf} all'"  
	emake PREFIX=/usr RELEASE=${PVR} ${myconf} all || die "emake failed"
}

src_install () {
	emake PREFIX=/usr cfg-target="/etc/sems/" DESTDIR="${D}" install || die
	newinitd "${FILESDIR}"/sems.rc6 sems
	newconfd "${FILESDIR}"/sems.confd sems
	dodoc README
}
