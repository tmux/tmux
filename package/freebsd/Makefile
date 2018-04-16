# $FreeBSD$
# This is adapted from the FreeBSD port, installing as "xterm-dev" with
# separate resource- and manpage files.
# copy "xterm-${PORTVERSION}.tgz to the port's distfiles directory
# and "make makesum".

PORTNAME=	xterm
PORTVERSION=	331
CATEGORIES=	x11
MASTER_SITES=	ftp://ftp.invisible-island.net/xterm/ \
		CRITICAL
PKGNAMESUFFIX=	-dev
EXTRACT_SUFX=	.tgz
DISTFILES=	${DISTNAME}${EXTRACT_SUFX}:src1 \
		bsd-xterm-icons-${ICONVERSION}${EXTRACT_SUFX}:src2

MAINTAINER=	ehaupt@FreeBSD.org
COMMENT=	Terminal emulator for the X Window System

CONFLICTS=	x11iraf-[0-9]*

USE_XORG=	xft
GNU_CONFIGURE=	yes

CONFIGURE_ARGS+=	--with-utempter --enable-narrowproto

CPPFLAGS+=	-I${LOCALBASE}/include
LIBS+=		-L${LOCALBASE}/lib

ICONVERSION=	1

OPTIONS_DEFINE=	WCHAR LUIT DECTERM PCRE PCRE2 DABBREV 256COLOR SIXEL GNOME LOGGING TOOLBAR
OPTIONS_RADIO=	3D
OPTIONS_RADIO_3D=	XAW3D XAW3DXFT NEXTAW

TOOLBAR_DESC=	Enable support menu/toolbar
DABBREV_DESC=	Enable support for dabbrev-expand
DECTERM_DESC=	Enable DECterm Locator support
LUIT_DESC=	Use LUIT for locale convertion from/to UTF-8
SIXEL_DESC=	Enable Sixel graphics support
WCHAR_DESC=	Enable wide-character support
256COLOR_DESC=	Enable 256-color support
LOGGING_DESC=	Enable logging terminal I/O to a file
XAW3D_DESC=	Link with Xaw 3d library
XAW3DXFT_DESC=	Link with Xaw 3d xft (extended fonts) library
NEXTAW_DESC=	Link with neXT Athena library

OPTIONS_DEFAULT=WCHAR LUIT 256COLOR

256COLOR_CONFIGURE_ENABLE=	256-color
LUIT_BUILD_DEPENDS=		luit:${PORTSDIR}/x11/luit
LUIT_CONFIGURE_ENABLE=		luit
WCHAR_CONFIGURE_ENABLE=		wide-chars
DECTERM_CONFIGURE_ENABLE=	dec-locator
PCRE_CONFIGURE_WITH=		pcre
PCRE_LIB_DEPENDS=		libpcre.so:${PORTSDIR}/devel/pcre
PCRE2_CONFIGURE_WITH=		pcre2
PCRE2_LIB_DEPENDS=		libpcre2-8.so:${PORTSDIR}/devel/pcre2
DABBREV_CONFIGURE_ENABLE=	dabbrev
SIXEL_CONFIGURE_ENABLE=		sixel-graphics
GNOME_USES=			desktop-file-utils
LOGGING_CONFIGURE_ENABLE=	logging
TOOLBAR_CONFIGURE_ENABLE=	toolbar
XAW3D_CONFIGURE_WITH=		Xaw3d
XAW3D_LIB_DEPENDS=		libXaw3d.so:${PORTSDIR}/x11-toolkits/Xaw3d
XAW3DXFT_CONFIGURE_WITH=	Xaw3dxft
XAW3DXFT_LIB_DEPENDS=		libXaw3dxft.so:${PORTSDIR}/x11-toolkits/libxaw3dxft
NEXTAW_CONFIGURE_WITH=		neXtaw
NEXTAW_LIB_DEPENDS=		libneXtaw.so:${PORTSDIR}/x11-toolkits/neXtaw

PKG_CLASS=	XTermDev

.include <bsd.port.options.mk>

CONFIGURE_ARGS+=	--program-suffix=${PKGNAMESUFFIX} --with-app-class=${PKG_CLASS} --without-xterm-symlink
MAN1=		resize${PKGNAMESUFFIX}.1 koi8rxterm${PKGNAMESUFFIX}.1 uxterm${PKGNAMESUFFIX}.1 xterm${PKGNAMESUFFIX}.1

.if ! ${PORT_OPTIONS:MXAW3D} && ! ${PORT_OPTIONS:MXAW3DXFT} && ! ${PORT_OPTIONS:MNEXTAW}
USE_XORG+=	xaw
.endif

.if ${OSVERSION} < 900004
LIB_DEPENDS+=	libutempter.so:${PORTSDIR}/sysutils/libutempter
.endif

post-extract:
	@${CP} ${WRKDIR}/bsd-xterm-icons-1/*.png \
		${WRKDIR}/bsd-xterm-icons-1/*.xpm \
			${WRKSRC}/icons/

post-install:
.for f in koi8rxterm${PKGNAMESUFFIX} resize${PKGNAMESUFFIX} uxterm${PKGNAMESUFFIX} xterm${PKGNAMESUFFIX}
	@${CHMOD} ${BINMODE} ${STAGEDIR}${PREFIX}/bin/${f}
.endfor
	@${INSTALL_DATA} ${WRKSRC}/xterm.desktop \
		${STAGEDIR}${PREFIX}/share/applications/xterm${PKGNAMESUFFIX}.desktop 

.if ${PORT_OPTIONS:MWCHAR}
	@${CAT} ${PKGMESSAGE}
.endif

.include <bsd.port.mk>
