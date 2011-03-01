#include "api_v1.h"


void
Api_v1::auth_1( QxtWebRequestEvent* event )
{
    qDebug() << "AUTH_1 HTTP" << event->url.toString();

    if ( !event->url.hasQueryItem( "website" ) || !event->url.hasQueryItem( "name" ) )
    {
        qDebug() << "Malformed HTTP resolve request";
        send404( event );
    }

    QString formToken = uuid();

    if ( event->url.hasQueryItem( "json" ) )
    {
        // JSON response
        QVariantMap m;
        m[ "formtoken" ] = formToken;
        sendJSON( m, event );
    }
    else
    {
        // webpage request
        QString authPage = RESPATH "www/auth.html";
        QHash< QString, QString > args;
        if( event->url.hasQueryItem( "receiverurl" ) )
            args[ "url" ] = QUrl::fromPercentEncoding( event->url.queryItemValue( "receiverurl" ).toUtf8() );

        args[ "formtoken" ] = formToken;
        args[ "website" ] = QUrl::fromPercentEncoding( event->url.queryItemValue( "website" ).toUtf8() );
        args[ "name" ] = QUrl::fromPercentEncoding( event->url.queryItemValue( "name" ).toUtf8() );
        sendWebpageWithArgs( event, authPage, args );
    }
}


void
Api_v1::auth_2( QxtWebRequestEvent* event )
{
    qDebug() << "AUTH_2 HTTP" << event->url.toString();
    QUrl url = event->url;
    url.setEncodedQuery( event->content->readAll() );

    if( !url.hasQueryItem( "website" ) || !url.hasQueryItem( "name" ) || !url.hasQueryItem( "formtoken" ) )
    {
        qDebug() << "Malformed HTTP resolve request";
        qDebug() << url.hasQueryItem( "website" )  << url.hasQueryItem( "name" )  << url.hasQueryItem( "formtoken" );
        send404( event );
        return;
    }

    QString website = QUrl::fromPercentEncoding( url.queryItemValue( "website" ).toUtf8() );
    QString name  = QUrl::fromPercentEncoding( url.queryItemValue( "name" ).toUtf8() );
    QByteArray authtoken = uuid().toLatin1();
    qDebug() << "HEADERS:" << event->headers;
    if( !url.hasQueryItem( "receiverurl" ) && url.queryItemValue( "receiverurl" ).isEmpty() )
    {
        //no receiver url, so do it ourselves
        QString receiverUrl = QUrl::fromPercentEncoding( url.queryItemValue( "receiverurl" ).toUtf8() );
        if( url.hasQueryItem( "json" ) )
        {
            QVariantMap m;
            m[ "authtoken" ] = authtoken;

            sendJSON( m, event );
        }
        else
        {
            QString authPage = RESPATH "www/auth.na.html";
            QHash< QString, QString > args;
            args[ "authcode" ] = authPage;
            args[ "website" ] = QUrl::fromPercentEncoding( url.queryItemValue( "website" ).toUtf8() );
            args[ "name" ] = QUrl::fromPercentEncoding( url.queryItemValue( "name" ).toUtf8() );
            sendWebpageWithArgs( event, authPage, args );
        }
    }
    else
    {
        // do what the client wants
        QUrl receiverurl = QUrl( url.queryItemValue( "receiverurl" ).toUtf8(), QUrl::TolerantMode );
        receiverurl.addEncodedQueryItem( "authtoken", "#" + authtoken );
        qDebug() << "Got receiver url:" << receiverurl.toString();

        QxtWebRedirectEvent* e = new QxtWebRedirectEvent( event->sessionID, event->requestID, receiverurl.toString() );
        postEvent( e );
        // TODO validation of receiverurl?
    }

    DatabaseCommand_AddClientAuth* dbcmd = new DatabaseCommand_AddClientAuth( authtoken, website, name, event->headers.key( "ua" ) );
    Database::instance()->enqueue( QSharedPointer<DatabaseCommand>(dbcmd) );
}


// all v1 api calls go to /api/
void
Api_v1::api( QxtWebRequestEvent* event )
{
    qDebug() << "HTTP" << event->url.toString();

    const QUrl& url = event->url;
    if( url.hasQueryItem( "method" ) )
    {
        const QString method = url.queryItemValue( "method" );

        if( method == "stat" )        return stat( event );
        if( method == "resolve" )     return resolve( event );
        if( method == "get_results" ) return get_results( event );
    }

    send404( event );
}


// request for stream: /sid/<id>
void
Api_v1::sid( QxtWebRequestEvent* event, QString unused )
{
    using namespace Tomahawk;
    RID rid = event->url.path().mid( 5 );
    qDebug() << "Request for sid " << rid;

    result_ptr rp = Pipeline::instance()->result( rid );
    if( rp.isNull() )
    {
        return send404( event );
    }

    QSharedPointer<QIODevice> iodev = Servent::instance()->getIODeviceForUrl( rp );
    if( iodev.isNull() )
    {
        return send404( event ); // 503?
    }

    QxtWebPageEvent* e = new QxtWebPageEvent( event->sessionID, event->requestID, iodev );
    e->streaming = iodev->isSequential();
    e->contentType = rp->mimetype().toAscii();
    e->headers.insert( "Content-Length", QString::number( rp->size() ) );
    postEvent( e );
}


void
Api_v1::send404( QxtWebRequestEvent* event )
{
    qDebug() << "404" << event->url.toString();
    QxtWebPageEvent* wpe = new QxtWebPageEvent( event->sessionID, event->requestID, "<h1>Not Found</h1>" );
    wpe->status = 404;
    wpe->statusMessage = "not feventound";
    postEvent( wpe );
}


void
Api_v1::stat( QxtWebRequestEvent* event )
{
    qDebug() << "Got Stat request:" << event->url.toString();
    m_storedEvent = event;

    if( !event->content.isNull() )
        qDebug() << "BODY:" << event->content->readAll();

    if( event->url.hasQueryItem( "auth" ) )
    {
        // check for auth status
        DatabaseCommand_ClientAuthValid* dbcmd = new DatabaseCommand_ClientAuthValid( event->url.queryItemValue( "auth" ) );
        connect( dbcmd, SIGNAL( authValid( QString, QString, bool ) ), this, SLOT( statResult( QString, QString, bool ) ) );
        Database::instance()->enqueue( QSharedPointer<DatabaseCommand>(dbcmd) );
    }
    else
    {
        statResult( QString(), QString(), false );
    }
}


void
Api_v1::statResult( const QString& clientToken, const QString& name, bool valid )
{
    QVariantMap m;
    m.insert( "name", "playdar" );
    m.insert( "version", "0.1.1" ); // TODO (needs to be >=0.1.1 for JS to work)
    m.insert( "authenticated", valid ); // TODO
    m.insert( "capabilities", QVariantList() );
    sendJSON( m, m_storedEvent );

    m_storedEvent = 0;
}


void
Api_v1::resolve( QxtWebRequestEvent* event )
{
    if( !event->url.hasQueryItem( "artist" ) ||
        !event->url.hasQueryItem( "track" ) )
    {
        qDebug() << "Malformed HTTP resolve request";
        send404( event );
    }

    QString qid;
    if ( event->url.hasQueryItem( "qid" ) )
        qid = event->url.queryItemValue( "qid" );
    else
        qid = uuid();

    Tomahawk::query_ptr qry = Tomahawk::Query::get( event->url.queryItemValue( "artist" ), event->url.queryItemValue( "track" ), event->url.queryItemValue( "album" ), qid );

    QVariantMap r;
    r.insert( "qid", qid );
    sendJSON( r, event );
}


void
Api_v1::staticdata( QxtWebRequestEvent* event, const QString& str )
{
    qDebug() << "STATIC request:" << event << str;
    if( str.contains( "tomahawk_auth_logo.png" ) )
    {
        QFile f( RESPATH "www/tomahawk_banner_small.png" );
        f.open( QIODevice::ReadOnly );
        QByteArray data = f.readAll();
        QxtWebPageEvent * e = new QxtWebPageEvent( event->sessionID, event->requestID, data );
        e->contentType = "image/png";
        postEvent( e );
    }
}


void
Api_v1::get_results( QxtWebRequestEvent* event )
{
    if( !event->url.hasQueryItem("qid") )
    {
        qDebug() << "Malformed HTTP get_results request";
        send404(event);
    }

    using namespace Tomahawk;
    query_ptr qry = Pipeline::instance()->query( event->url.queryItemValue( "qid" ) );
    if( qry.isNull() )
    {
        send404( event );
        return;
    }

    QVariantMap r;
    r.insert( "qid", qry->id() );
    r.insert( "poll_interval", 1000 );
    r.insert( "refresh_interval", 1000 );
    r.insert( "poll_limit", 6 );
    r.insert( "solved", qry->solved() );
    r.insert( "query", qry->toVariant() );

    QVariantList res;
    foreach( Tomahawk::result_ptr rp, qry->results() )
    {
        res << rp->toVariant();
    }
    r.insert( "results", res );

    sendJSON( r, event );
}


void
Api_v1::sendJSON( const QVariantMap& m, QxtWebRequestEvent* event )
{
    QJson::Serializer ser;
    QByteArray ctype;
    QByteArray body = ser.serialize( m );

    if( event->url.hasQueryItem("jsonp") && !event->url.queryItemValue( "jsonp" ).isEmpty() )
    {
        ctype = "text/javascript; charset=utf-8";
        body.prepend( QString("%1( ").arg( event->url.queryItemValue( "jsonp" ) ).toAscii() );
        body.append( " );" );
    }
    else
    {
        ctype = "appplication/json; charset=utf-8";
    }

    QxtWebPageEvent * e = new QxtWebPageEvent( event->sessionID, event->requestID, body );
    e->contentType = ctype;
    e->headers.insert( "Content-Length", QString::number( body.length() ) );
    postEvent( e );
    qDebug() << "JSON response" << event->url.toString() << body;
}


// load an html template from a file, replace args from map
// then serve
void
Api_v1::sendWebpageWithArgs( QxtWebRequestEvent* event, const QString& filenameSource, const QHash< QString, QString >& args )
{
    if( !QFile::exists( filenameSource ) )
        qWarning() << "Passed invalid file for html source:" << filenameSource;

    QFile f( filenameSource );
    f.open( QIODevice::ReadOnly );
    QByteArray html = f.readAll();

    foreach( const QString& param, args.keys() )
    {
        html.replace( QString( "<%%1%>" ).arg( param.toUpper() ), args.value( param ).toUtf8() );
    }

    QxtWebPageEvent* e = new QxtWebPageEvent( event->sessionID, event->requestID, html );
    postEvent( e );
}


void
Api_v1::index( QxtWebRequestEvent* event )
{
    send404( event );
}
