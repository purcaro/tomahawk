/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2010-2011, Leo Franchi <lfranchi@kde.org>
 *   Copyright 2012,      Teo Mrnjavac <teo@kde.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TomahawkUtilsGui.h"

#include "playlist/PlayableItem.h"
#include "config.h"
#include "Query.h"
#include "Result.h"
#include "Source.h"
#include "ImageRegistry.h"
#include "Logger.h"

#include <QLayout>
#include <QPainter>
#include <QPixmap>
#include <QBitmap>
#include <QPalette>
#include <QApplication>
#include <QScrollBar>
#include <QWidget>
#include <QStyleOption>
#include <QDesktopServices>

#ifdef Q_WS_X11
    #include <QtGui/QX11Info>
    #include <libqnetwm/netwm.h>
#endif

#ifdef Q_WS_WIN
    #include <windows.h>
    #include <windowsx.h>
    #include <shellapi.h>
#endif

#ifdef QT_MAC_USE_COCOA
    #include "widgets/SourceTreePopupDialog_mac.h"
#endif

// Defined in qpixmapfilter.cpp, private but exported
extern void qt_blurImage( QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );

namespace TomahawkUtils
{
static int s_defaultFontSize = 0;
static int s_defaultFontHeight = 0;


QPixmap
createDragPixmap( MediaType type, int itemCount )
{
    // If more than one item is dragged, align the items inside a
    // rectangular grid. The maximum grid size is limited to 5 x 5 items.
    int xCount = 3;
    int size = 32;

    if ( itemCount > 16 )
    {
        xCount = 5;
        size = 16;
    }
    else if( itemCount > 9 )
    {
        xCount = 4;
        size = 22;
    }

    if ( itemCount < xCount )
    {
        xCount = itemCount;
    }

    int yCount = itemCount / xCount;
    if ( itemCount % xCount != 0 )
    {
        ++yCount;
    }
    if ( yCount > xCount )
    {
        yCount = xCount;
    }
    // Draw the selected items into the grid cells
    QPixmap dragPixmap( xCount * size + xCount - 1, yCount * size + yCount - 1 );
    dragPixmap.fill( Qt::transparent );

    QPainter painter( &dragPixmap );
    painter.setRenderHint( QPainter::Antialiasing );

    QPixmap pixmap;
    switch ( type )
    {
        case MediaTypeArtist:
            pixmap = TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultArtistImage, TomahawkUtils::Original, QSize( size, size ) );
            break;
        case MediaTypeAlbum:
            pixmap = TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultAlbumCover, TomahawkUtils::Original, QSize( size, size ) );
            break;
        case MediaTypeTrack:
            pixmap = TomahawkUtils::defaultPixmap( TomahawkUtils::DefaultTrackImage, TomahawkUtils::Original, QSize( size, size ) );
            break;
    }

    int x = 0;
    int y = 0;
    for ( int i = 0; i < itemCount; ++i )
    {

        painter.drawPixmap( x, y, pixmap );

        x += size + 1;
        if ( x >= dragPixmap.width() )
        {
            x = 0;
            y += size + 1;
        }
        if ( y >= dragPixmap.height() )
        {
            break;
        }
    }

    return dragPixmap;
}


void
drawShadowText( QPainter* painter, const QRect& rect, const QString& text, const QTextOption& textOption )
{
    painter->save();

    painter->drawText( rect, text, textOption );

/*    QFont font = painter->font();
    font.setPixelSize( font.pixelSize() + 2 );
    painter->setFont( font );

    painter->setPen( Qt::black );
    painter->drawText( rect, text, textOption );*/

    painter->restore();
}


void
drawBackgroundAndNumbers( QPainter* painter, const QString& text, const QRect& figRectIn )
{
    painter->save();

    QRect figRect = figRectIn;
    if ( text.length() == 1 )
        figRect.adjust( -painter->fontMetrics().averageCharWidth(), 0, 0, 0 );

    QPen origpen = painter->pen();
    QPen pen = origpen;
    pen.setWidth( 1.0 );
    painter->setPen( pen );
    painter->drawRect( figRect );

    // circles look bad. make it an oval. (thanks, apple)
    const int bulgeWidth = 8;
    const int offset = 0; // number of pixels to begin, counting inwards from figRect.x() and figRect.width(). 0 means start at each end, negative means start inside the rect.

    QPainterPath ppath;
    ppath.moveTo( QPoint( figRect.x() + offset, figRect.y() + figRect.height() / 2 ) );
    QRect leftArcRect( figRect.x() + offset - bulgeWidth, figRect.y(), 2*bulgeWidth, figRect.height() );
    ppath.arcTo( leftArcRect, 90, 180 );
    painter->drawPath( ppath );

    ppath = QPainterPath();
    ppath.moveTo( figRect.x() + figRect.width() - offset, figRect.y() + figRect.height() / 2 );
    leftArcRect = QRect( figRect.x() + figRect.width() - offset - bulgeWidth, figRect.y(), 2*bulgeWidth, figRect.height() );
    ppath.arcTo( leftArcRect, 270, 180 );
    painter->drawPath( ppath );

    figRect.adjust( -1, 0, 0, 0 );

    painter->setPen( origpen );
    painter->setPen( Qt::white );
    painter->drawText( figRect.adjusted( -5, 2, 6, 0 ), text, QTextOption( Qt::AlignCenter ) );

    painter->restore();
}


void
drawQueryBackground( QPainter* p, const QPalette& palette, const QRect& r, qreal lightnessFactor )
{
    p->setPen( palette.highlight().color().lighter( lightnessFactor * 100 ) );
    p->setBrush( palette.highlight().color().lighter( lightnessFactor * 100 ) );
    p->drawRoundedRect( r, 4.0, 4.0 );
}


void
unmarginLayout( QLayout* layout )
{
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setMargin( 0 );
    layout->setSpacing( 0 );

    for ( int i = 0; i < layout->count(); i++ )
    {
        QLayout* childLayout = layout->itemAt( i )->layout();
        if ( childLayout )
            unmarginLayout( childLayout );
    }
}


QWidget*
tomahawkWindow()
{
    QWidgetList widgetList = qApp->topLevelWidgets();
    int i = 0;
    while( i < widgetList.count() && widgetList.at( i )->objectName() != "TH_Main_Window" )
        i++;

    if ( i == widgetList.count() )
    {
        qDebug() << Q_FUNC_INFO << "could not find main Tomahawk mainwindow";
        Q_ASSERT( false );
        return 0;
    }

    QWidget *widget = widgetList.at( i );
    return widget;
}


#ifndef Q_WS_MAC
void
bringToFront()
{
#if defined(Q_WS_X11)
    {
        qDebug() << Q_FUNC_INFO;

        QWidget* widget = tomahawkWindow();
        if ( !widget )
            return;

        widget->show();
        widget->activateWindow();
        widget->raise();

        WId wid = widget->winId();
        NETWM::init();

        XEvent e;
        e.xclient.type = ClientMessage;
        e.xclient.message_type = NETWM::NET_ACTIVE_WINDOW;
        e.xclient.display = QX11Info::display();
        e.xclient.window = wid;
        e.xclient.format = 32;
        e.xclient.data.l[0] = 2;
        e.xclient.data.l[1] = QX11Info::appTime();
        e.xclient.data.l[2] = 0;
        e.xclient.data.l[3] = 0l;
        e.xclient.data.l[4] = 0l;

        XSendEvent( QX11Info::display(), RootWindow( QX11Info::display(), DefaultScreen( QX11Info::display() ) ), False, SubstructureRedirectMask | SubstructureNotifyMask, &e );
    }
#elif defined(Q_WS_WIN)
    {
        qDebug() << Q_FUNC_INFO;

        QWidget* widget = tomahawkWindow();
        if ( !widget )
            return;

        widget->show();
        widget->activateWindow();
        widget->raise();

        WId wid = widget->winId();

        HWND hwndActiveWin = GetForegroundWindow();
        int  idActive      = GetWindowThreadProcessId(hwndActiveWin, NULL);
        if ( AttachThreadInput(GetCurrentThreadId(), idActive, TRUE) )
        {
            SetForegroundWindow( wid );
            SetFocus( wid );
            AttachThreadInput(GetCurrentThreadId(), idActive, FALSE);
        }
    }
#endif
}
#endif


void
openUrl( const QUrl& url )
{
#ifdef Q_OS_WIN
    ShellExecuteW( 0, 0, (LPCWSTR)url.toString().utf16(), 0, 0, SW_SHOWNORMAL );
#else
    QDesktopServices::openUrl( url );
#endif
}


QPixmap
createRoundedImage( const QPixmap& pixmap, const QSize& size, float frameWidthPct )
{
    int height;
    int width;

    if ( !size.isEmpty() )
    {
        height = size.height();
        width = size.width();
    }
    else
    {
        height = pixmap.height();
        width = pixmap.width();
    }

    if ( !height || !width )
        return QPixmap();

    QPixmap scaledAvatar = pixmap.scaled( width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
    if ( frameWidthPct == 0.00 )
        return scaledAvatar;

    QPixmap frame( width, height );
    frame.fill( Qt::transparent );

    QPainter painter( &frame );
    painter.setRenderHint( QPainter::Antialiasing );

    QRect outerRect( 0, 0, width, height );
    QBrush brush( scaledAvatar );
    QPen pen;
    pen.setColor( Qt::transparent );
    pen.setJoinStyle( Qt::RoundJoin );

    painter.setBrush( brush );
    painter.setPen( pen );
    painter.drawRoundedRect( outerRect, frameWidthPct * 100.0, frameWidthPct * 100.0, Qt::RelativeSize );

/*    painter.setBrush( Qt::transparent );
    painter.setPen( Qt::white );
    painter.drawRoundedRect( outerRect, frameWidthPct, frameWidthPct, Qt::RelativeSize ); */

    return frame;
}


int
defaultFontSize()
{
    return s_defaultFontSize;
}


int
defaultFontHeight()
{
    if ( s_defaultFontHeight <= 0 )
    {
        QFont f;
        f.setPointSize( defaultFontSize() );
        s_defaultFontHeight = QFontMetrics( f ).height();
    }

    return s_defaultFontHeight;
}


void
setDefaultFontSize( int points )
{
    s_defaultFontSize = points;
}


QSize
defaultIconSize()
{
    const int w = defaultFontHeight() * 1.6;
    return QSize( w, w );
}


QColor
alphaBlend( const QColor& colorFrom, const QColor& colorTo, float opacity )
{
    opacity = qMax( (float)0.3, opacity );
    int r = colorFrom.red(), g = colorFrom.green(), b = colorFrom.blue();
    r = opacity * r + ( 1 - opacity ) * colorTo.red();
    g = opacity * g + ( 1 - opacity ) * colorTo.green();
    b = opacity * b + ( 1 - opacity ) * colorTo.blue();

    return QColor( r, g, b );
}


QString
defaultImagePath( ImageType type, ImageMode mode )
{
    switch ( type )
    {
        case DefaultAlbumCover:
            if ( mode == CoverInCase )
                return QLatin1String( RESPATH "images/no-album-art-placeholder.svg" );
            else if ( mode == Grid )
                return QLatin1String( RESPATH "images/album-placeholder-grid.svg" );
            else
                return QLatin1String( RESPATH "images/album-icon.svg" );
            break;

        case DefaultArtistImage:
            if ( mode == Grid )
                return QLatin1String( RESPATH "images/artist-placeholder-grid.svg" );
            else
                return QLatin1String( RESPATH "images/artist-icon.svg" );
            break;

        case DefaultTrackImage:
            if ( mode == Grid )
                pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/track-placeholder-grid.svg", size );
            else if ( mode == RoundedCorners )
                pixmap = ImageRegistry::instance()->pixmap( RESPATH "images/track-icon.svg", size, TomahawkUtils::RoundedCorners );
            else
                return QLatin1String( RESPATH "images/track-icon.svg" );
            break;

        case DefaultSourceAvatar:
            return QLatin1String( RESPATH "images/user-avatar.svg" );
            break;

        case DefaultResolver:
            return QLatin1String( RESPATH "images/resolver-default.svg" );
            break;

        case DefaultCollection:
            return QLatin1String( RESPATH "images/collection.svg" );
            break;

        case NowPlayingSpeaker:
            return QLatin1String( RESPATH "images/now-playing-speaker.svg" );
            break;

        case NowPlayingSpeakerDark:
            return QLatin1String( RESPATH "images/now-playing-speaker-dark.svg" );
            break;

        case InfoIcon:
            return QLatin1String( RESPATH "images/info.svg" );
            break;

        case PlayButton:
            return QLatin1String( RESPATH "images/play-rest.svg" );
            break;
        case PlayButtonPressed:
            return QLatin1String( RESPATH "images/play-pressed.svg" );
            break;

        case PauseButton:
            return QLatin1String( RESPATH "images/pause-rest.svg" );
            break;
        case PauseButtonPressed:
            return QLatin1String( RESPATH "images/pause-pressed.svg" );
            break;

        case PrevButton:
            return QLatin1String( RESPATH "images/back-rest.svg" );
            break;
        case PrevButtonPressed:
            return QLatin1String( RESPATH "images/back-pressed.svg" );
            break;

        case NextButton:
            return QLatin1String( RESPATH "images/skip-rest.svg" );
            break;
        case NextButtonPressed:
            return QLatin1String( RESPATH "images/skip-pressed.svg" );
            break;

        case ShuffleOff:
            return QLatin1String( RESPATH "images/shuffle-off-rest.svg" );
            break;
        case ShuffleOffPressed:
            return QLatin1String( RESPATH "images/shuffle-off-pressed.svg" );
            break;
        case ShuffleOn:
            return QLatin1String( RESPATH "images/shuffle-on-rest.svg" );
            break;
        case ShuffleOnPressed:
            return QLatin1String( RESPATH "images/shuffle-on-pressed.svg" );
            break;

        case RepeatOne:
            return QLatin1String( RESPATH "images/repeat-1-on-rest.svg" );
            break;
        case RepeatOnePressed:
            return QLatin1String( RESPATH "images/repeat-1-on-pressed.svg" );
            break;
        case RepeatAll:
            return QLatin1String( RESPATH "images/repeat-all-on-rest.svg" );
            break;
        case RepeatAllPressed:
            return QLatin1String( RESPATH "images/repeat-all-on-pressed.svg" );
            break;
        case RepeatOff:
            return QLatin1String( RESPATH "images/repeat-off-rest.svg" );
            break;
        case RepeatOffPressed:
            return QLatin1String( RESPATH "images/repeat-off-pressed.svg" );
            break;

        case VolumeMuted:
            return QLatin1String( RESPATH "images/volume-icon-muted.svg" );
            break;
        case VolumeFull:
            return QLatin1String( RESPATH "images/volume-icon-full.svg" );
            break;

        case Share:
            return QLatin1String( RESPATH "images/share.svg" );
            break;

        case NotLoved:
            return QLatin1String( RESPATH "images/not-loved.svg" );
            break;
        case Loved:
            return QLatin1String( RESPATH "images/loved.svg" );
            break;

        case Configure:
            return QLatin1String( RESPATH "images/configure.svg" );
            break;

        case GreenDot:
            return QLatin1String( RESPATH "images/green-dot.svg" );
            break;

        case AddContact:
            return QLatin1String( RESPATH "images/add-contact.svg" );
            break;

        case SubscribeOn:
            return QLatin1String( RESPATH "images/subscribe-on.svg" );
            break;
        case SubscribeOff:
            return QLatin1String( RESPATH "images/subscribe-off.svg" );
            break;

        case JumpLink:
            return QLatin1String( RESPATH "images/jump-link.svg" );
            break;

        case ProcessStop:
            return QLatin1String( RESPATH "images/process-stop.svg" );
            break;

        case HeadphonesOn:
            return QLatin1String( RESPATH "images/headphones.svg" );
            break;
        case HeadphonesOff:
            return QLatin1String( RESPATH "images/headphones-off.svg" );
            break;

        case PadlockClosed:
            return QLatin1String( RESPATH "images/closed-padlock.svg" );
            break;
        case PadlockOpen:
            return QLatin1String( RESPATH "images/open-padlock.svg" );
            break;

        case Downloading:
            return QLatin1String( RESPATH "images/downloading.svg" );
            break;
        case Uploading:
            return QLatin1String( RESPATH "images/uploading.svg" );
            break;

        case ViewRefresh:
            return QLatin1String( RESPATH "images/view-refresh.svg" );
            break;

        case SuperCollection:
            return QLatin1String( RESPATH "images/supercollection.svg" );
            break;
        case LovedPlaylist:
            return QLatin1String( RESPATH "images/loved_playlist.svg" );
            break;
        case NewReleases:
            return QLatin1String( RESPATH "images/new-releases.svg" );
            break;
        case NewAdditions:
            return QLatin1String( RESPATH "images/new-additions.svg" );
            break;
        case RecentlyPlayed:
            return QLatin1String( RESPATH "images/recently-played.svg" );
            break;
        case Charts:
            return QLatin1String( RESPATH "images/charts.svg" );
            break;
        case AutomaticPlaylist:
            return QLatin1String( RESPATH "images/automatic-playlist.svg" );
            break;
        case Station:
            return QLatin1String( RESPATH "images/station.svg" );
            break;
        case Playlist:
            return QLatin1String( RESPATH "images/playlist-icon.svg" );
            break;
        case Search:
            return QLatin1String( RESPATH "images/search-icon.svg" );
            break;

        case Add:
            return QLatin1String( RESPATH "images/add.svg" );
            break;
        case ListAdd:
            return QLatin1String( RESPATH "images/list-add.svg" );
            break;
        case ListRemove:
            return QLatin1String( RESPATH "images/list-remove.svg" );
            break;

        case AdvancedSettings:
            return QLatin1String( RESPATH "images/advanced-settings.svg" );
            break;
        case AccountSettings:
            return QLatin1String( RESPATH "images/account-settings.svg" );
            break;
        case MusicSettings:
            return QLatin1String( RESPATH "images/music-settings.svg" );
            break;

        case DropSong:
            return QLatin1String( RESPATH "images/drop-song.svg" );
            break;
        case DropAlbum:
            return QLatin1String( RESPATH "images/drop-album.svg" );
            break;
        case DropAllSongs:
            return QLatin1String( RESPATH "images/drop-all-songs.svg" );
            break;
        case DropLocalSongs:
            return QLatin1String( RESPATH "images/drop-local-songs.svg" );
            break;
        case DropTopSongs:
            return QLatin1String( RESPATH "images/drop-top-songs.svg" );
            break;

        case Starred:
            return QLatin1String( RESPATH "images/starred.svg" );
            break;
        case Unstarred:
            return QLatin1String( RESPATH "images/star-unstarred.svg" );
            break;
        case StarHovered:
            return QLatin1String( RESPATH "images/star-hover.svg" );
            break;

        case SipPluginOnline:
            return QLatin1String( RESPATH "images/sipplugin-online.svg" );
            break;
        case SipPluginOffline:
            return QLatin1String( RESPATH "images/sipplugin-offline.svg" );
            break;

        case AccountNone:
            return QLatin1String( RESPATH "images/account-none.svg" );
            break;
        case LastfmIcon:
            return QLatin1String( RESPATH "images/lastfm-icon.svg" );
            break;
        case SpotifyIcon:
            return QLatin1String( RESPATH "images/spotify-sourceicon.svg" );
            break;
        case SoundcloudIcon:
            return QLatin1String( RESPATH "images/soundcloud.svg" );
            break;

        default:
            break;
    }

    Q_ASSERT(false);
    return QString();
}


QPixmap
defaultPixmap( ImageType type, ImageMode mode, const QSize& size )
{
    QPixmap pixmap;
    if ( mode == RoundedCorners ) {
        pixmap = ImageRegistry::instance()->pixmap( defaultImagePath( type, mode ), size, TomahawkUtils::RoundedCorners );
    } else {
        pixmap = ImageRegistry::instance()->pixmap( defaultImagePath( type, mode ), size );
    }

    if ( pixmap.isNull() )
    {
        Q_ASSERT( false );
        return QPixmap();
    }

    return pixmap;

}


void
prepareStyleOption( QStyleOptionViewItemV4* option, const QModelIndex& index, PlayableItem* item )
{
    Q_UNUSED( index );

    if ( item->isPlaying() )
    {
        option->backgroundBrush = TomahawkUtils::Colors::NOW_PLAYING_ITEM;
        option->palette.setColor( QPalette::Highlight, TomahawkUtils::Colors::NOW_PLAYING_ITEM.lighter() );
        option->palette.setColor( QPalette::Text, TomahawkUtils::Colors::NOW_PLAYING_ITEM_TEXT );

    }
    else if ( option->state & QStyle::State_Selected )
    {
        option->palette.setColor( QPalette::Text, option->palette.color( QPalette::HighlightedText ) );
    }
    else
    {
        float opacity = 0.0;
        if ( !item->query()->results().isEmpty() )
            opacity = item->query()->results().first()->score();

        opacity = qMax( (float)0.3, opacity );
        QColor textColor = alphaBlend( option->palette.color( QPalette::Text ), option->palette.color( QPalette::BrightText ), opacity );

        option->palette.setColor( QPalette::Text, textColor );
    }
}


void
drawRoundedButton( QPainter* painter, const QRect& btnRect, const QColor& color, const QColor &gradient1bottom, const QColor& gradient2top, const QColor& gradient2bottom )
{
    QPainterPath btnPath;
    const int radius = 3;
    // draw top half gradient
    const int btnCenter = btnRect.bottom() - ( btnRect.height() / 2 );
    btnPath.moveTo( btnRect.left(), btnCenter );
    btnPath.lineTo( btnRect.left(), btnRect.top() + radius );
    btnPath.quadTo( QPoint( btnRect.topLeft() ), QPoint( btnRect.left() + radius, btnRect.top() ) );
    btnPath.lineTo( btnRect.right() - radius, btnRect.top() );
    btnPath.quadTo( QPoint( btnRect.topRight() ), QPoint( btnRect.right(), btnRect.top() + radius ) );
    btnPath.lineTo( btnRect.right(),btnCenter );
    btnPath.lineTo( btnRect.left(), btnCenter );

    QLinearGradient g;
    if ( gradient1bottom.isValid() )
    {
        g.setColorAt( 0, color );
        g.setColorAt( 0.5, gradient1bottom );
        painter->fillPath( btnPath, g );
    }
    else
        painter->fillPath( btnPath, color );
    //painter->setPen( bg.darker() );

    //painter->drawPath( btnPath );

    btnPath = QPainterPath();
    btnPath.moveTo( btnRect.left(), btnCenter );
    btnPath.lineTo( btnRect.left(), btnRect.bottom() - radius );
    btnPath.quadTo( QPoint( btnRect.bottomLeft() ), QPoint( btnRect.left() + radius, btnRect.bottom() ) );
    btnPath.lineTo( btnRect.right() - radius, btnRect.bottom() );
    btnPath.quadTo( QPoint( btnRect.bottomRight() ), QPoint( btnRect.right(), btnRect.bottom() - radius ) );
    btnPath.lineTo( btnRect.right(), btnCenter );
    btnPath.lineTo( btnRect.left(), btnCenter );

    if ( gradient2top.isValid() && gradient2bottom.isValid() )
    {
        g.setColorAt( 0, gradient2top );
        g.setColorAt( 0.5, gradient2bottom );
        painter->fillPath( btnPath, g );
    }
    else
        painter->fillPath( btnPath, color );

}


void
styleScrollBar( QScrollBar* scrollBar )
{
    scrollBar->setStyleSheet(
        "QScrollBar:horizontal { background-color: transparent; }"
        "QScrollBar::handle:horizontal { border-height: 9px; margin-bottom: 6px;"
            "border-image: url(" RESPATH "images/scrollbar-horizontal-handle.png) 3 3 3 3 stretch stretch;"
            "border-top: 3px transparent; border-bottom: 3px transparent; border-right: 3px transparent; border-left: 3px transparent; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { width: 0px; height: 0px; background: none; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; height: 0px; background: none; }"
        "QScrollBar:left-arrow:horizontal, QScrollBar::right-arrow:horizontal {"
            "border: 0px; width: 0px; height: 0px; background: none; background-color: transparent; }"

        "QScrollBar:vertical { background-color: transparent; }"
        "QScrollBar::handle:vertical { border-width: 9px; margin-right: 6px;"
            "border-image: url(" RESPATH "images/scrollbar-vertical-handle.png) 3 3 3 3 stretch stretch;"
            "border-top: 3px transparent; border-bottom: 3px transparent; border-right: 3px transparent; border-left: 3px transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { width: 0px; height: 0px; background: none; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { width: 0px; height: 0px; background: none; }"
        "QScrollBar:up-arrow:vertical, QScrollBar::down-arrow:vertical {"
            "border: 0px; width: 0px; height: 0px; background: none; background-color: transparent; }" );
}


QPixmap
createTiledPixmap( int width, int height, const QImage& inputTile )
{
    if ( inputTile.isNull() )
        return QPixmap();


    QImage localTile = inputTile;

    if ( localTile.height() < height )
    {
        // image must be at least as tall as we are
        QImage taller( localTile.width(), height, QImage::Format_ARGB32_Premultiplied );
        QPainter p( &taller );
        int curY = 0;
        while ( curY < taller.height() )
        {
            const int thisHeight = (curY + localTile.height() > height) ? height - curY : localTile.height();
            p.drawImage( QRect( 0, curY, localTile.width(), thisHeight ), localTile, QRect( 0, 0, localTile.width(), thisHeight ) );
            curY += localTile.height();
        }
        localTile = taller;
    }


    QPixmap tiledImage = QPixmap( width, height );
    tiledImage.fill( Qt::transparent );

    int curWidth = 0;
    QPainter p( &tiledImage );
    while ( curWidth < width )
    {
        const int thisWidth = (curWidth + localTile.width() > width) ? width - curWidth : localTile.width();

        const QRect source( 0, 0, thisWidth, tiledImage.height() );
        const QRect dest( curWidth, 0, thisWidth, tiledImage.height() );
        p.drawImage( dest, localTile, source );
        curWidth += thisWidth;
    }

    return tiledImage;
}


//  addDropShadow is inspired by QPixmapDropShadowFilter::draw in
//  qt/src/gui/effects/qpixmapfilter.cpp
QPixmap
addDropShadow( const QPixmap& source, const QSize& targetSize )
{
    const QPoint offset( 2, 4 );
    const int radius = 4;
    const QColor shadowColor( 100, 100, 100, 100 );

    // If there is no targetSize, then return a larger pixmap with the shadow added on
    // otherwise, return a bounded pixmap and shrink the source
    const QSize sizeToUse = targetSize.isEmpty() ? QSize( source.width() + offset.x() + radius, source.height() + offset.y() + radius ) : targetSize;
    const QSize shrunkToFit( sizeToUse.width() - offset.x() - radius, sizeToUse.height() - offset.y() - radius );
    const QPixmap shrunk = source.scaled( shrunkToFit, Qt::KeepAspectRatio, Qt::SmoothTransformation );

    QImage tmp( sizeToUse, QImage::Format_ARGB32_Premultiplied );
    tmp.fill( 0 );

    QPainter tmpPainter( &tmp );
    tmpPainter.setCompositionMode( QPainter::CompositionMode_Source );
    tmpPainter.drawPixmap( offset, shrunk );
    tmpPainter.end();

    // blur the alpha channel
    QImage blurred( sizeToUse, QImage::Format_ARGB32_Premultiplied );
    blurred.fill( 0 );
    QPainter blurPainter( &blurred );
    qt_blurImage( &blurPainter, tmp, radius, false, true );
    blurPainter.end();

    // blacken the image...
    QPainter blackenPainter( &blurred );
    blackenPainter.setCompositionMode( QPainter::CompositionMode_SourceIn );
    blackenPainter.fillRect( blurred.rect(), shadowColor );
    blackenPainter.end();

    const QRect resultRect( shrunk.rect().united( shrunk.rect().translated( offset ).adjusted( -radius, -radius, radius, radius ) ) );

    QPixmap result( resultRect.size() );
    result.fill( Qt::transparent );
    QPainter resultPainter( &result );

    // draw the blurred drop shadow...
    resultPainter.drawImage( 0, 0, blurred );

    // Draw the actual pixmap...
    resultPainter.drawPixmap( 0, 0, shrunk );

    return result;
}


QPixmap
squareCenterPixmap( const QPixmap& sourceImage )
{
    if ( sourceImage.width() != sourceImage.height() )
    {
        const int sqwidth = qMin( sourceImage.width(), sourceImage.height() );
        const int delta = abs( sourceImage.width() - sourceImage.height() );

        if ( sourceImage.width() > sourceImage.height() )
        {
            return sourceImage.copy( delta / 2, 0, sqwidth, sqwidth );
        }
        else
        {
            return sourceImage.copy( 0, delta / 2, sqwidth, sqwidth );
        }
    }

    return sourceImage;
}


void
drawCompositedPopup( QWidget* widget,
                     const QPainterPath& outline,
                     const QColor& lineColor,
                     const QBrush& backgroundBrush,
                     qreal opacity )
{
    bool compositingWorks = true;
#if defined(Q_WS_WIN)   //HACK: Windows refuses to perform compositing so we must fake it
    compositingWorks = false;
#elif defined(Q_WS_X11)
    if ( !QX11Info::isCompositingManagerRunning() )
        compositingWorks = false;
#endif

    QPainter p;
    QImage result;
    if ( compositingWorks )
    {
        p.begin( widget );
        p.setRenderHint( QPainter::Antialiasing );
        p.setBackgroundMode( Qt::TransparentMode );
    }
    else
    {
        result =  QImage( widget->rect().size(), QImage::Format_ARGB32_Premultiplied );
        p.begin( &result );
        p.setCompositionMode( QPainter::CompositionMode_Source );
        p.fillRect( result.rect(), Qt::transparent );
        p.setCompositionMode( QPainter::CompositionMode_SourceOver );
    }

    QPen pen( lineColor );
    pen.setWidth( 2 );
    p.setPen( pen );
    p.drawPath( outline );

    p.setOpacity( opacity );
    p.fillPath( outline, backgroundBrush );
    p.end();

    if ( !compositingWorks )
    {
        QPainter finalPainter( widget );
        finalPainter.setRenderHint( QPainter::Antialiasing );
        finalPainter.setBackgroundMode( Qt::TransparentMode );
        finalPainter.drawImage( 0, 0, result );
        widget->setMask( QPixmap::fromImage( result ).mask() );
    }

#ifdef QT_MAC_USE_COCOA
    // Work around bug in Qt/Mac Cocoa where opening subsequent popups
    // would incorrectly calculate the background due to it not being
    // invalidated.
    SourceTreePopupHelper::clearBackground( widget );
#endif
}


} // ns
