import ekg
import time

def status_handler(session, uid, status, desc):
    for sesja in ekg.sessions():
	if sesja.connected():
	    ekg.echo("sesja '%s' połączona" % (name,))
	    ekg.echo("status: "+sesja['status'])
	else:
	    ekg.echo("sesja '%s' nie jest połączona" % (name,))
    ekg.echo("Dostałem status!")
    ekg.echo("Sesja : "+session)
    ekg.echo("UID   : "+uid)
    ekg.echo("Status: "+status)
    if desc:
	ekg.echo("Opis  : "+desc)
    sesja = ekg.session_get(session)
    ekg.echo('Lista userów sesji: '+", ".join(sesja.users()))
    user = sesja.user_get(uid)
    if user.last_status:
	ekg.echo(str(user.last_status))
	stat, des = user.last_status
	ekg.echo("Ostatni status: "+stat)
	if user.last_status[1]:
	    ekg.echo("Ostatni opis  : "+des)
    else:
	ekg.echo("Nie ma poprzedniego stanu - pewnie dopiero się łączymy...")
    ekg.echo("IP: "+user.ip)
    ekg.echo("Grupy: "+", ".join(user.groups()))
    if status == ekg.STATUS_AWAY:
	ekg.echo("Chyba go nie ma...")
    if status == ekg.STATUS_XA:
	ekg.echo("Chyba bardzo go nie ma, to na grzyb mi taki status?. Połykam. *ślurp*")
	return 0
    return 1

def message_handler(session, uid, type, text, sent_time, ignore_level):
    ekg.debug("[test script] some debug\n")
    ekg.echo("Dostałem wiadomość!")
    ekg.echo("Sesja : "+session)
    ekg.echo("UID   : "+uid)
    if type == ekg.MSGCLASS_MESSAGE:
	ekg.echo("Typ   : msg")
    elif type == ekg.MSGCLASS_CHAT:
	ekg.echo("Typ   : chat")
    ekg.echo("Czas  : "+time.strftime("%a, %d %b %Y %H:%M:%S %Z", time.gmtime(sent_time)))
    ekg.echo("Ign   : "+str(ignore_level))
    ekg.echo("TxtLen: "+str(len(text)))
    if len(text) == 13:
	ekg.echo("Oj, ale pechowa liczba, nie odbieram")
	return 0
    return 1

def own_message_handler(session, target, text):
    ekg.debug("[test script] some debug\n")
    ekg.echo("Wysyłam wiadomość!")
    ekg.echo("Sesja : "+session)
    ekg.echo("Target: "+target)
    ekg.echo("TxtLen: "+str(len(text)))
    return 1

def connect_handler(session):
    ekg.echo("Połączono! Ale super! Można gadać!")
    ekg.echo("Sesja : "+session)
    if session[:3] == 'irc':
	struct = time.gmtime()
	if struct[3] >= 8 and struct[3] < 17:
	    ekg.echo('Ładnie to tak ircować w pracy? ;)')
    sesja = ekg.session_get(session)
    if sesja.connected():
	ekg.echo('Połączony!')
    else:
	ekg.echo('W tym miejscu jeszcze nie połączony')
    ekg.echo('Lista userów sesji: '+", ".join(sesja.users()))

def keypress(key):
    ekg.echo('nacisnales #'+ str(key));
    

def disconnect_handler(session):
    ekg.echo("Ło, sesja %s nam padła" % (session,))
    ekg.echo("Wysyłamy smsa że nam cuś padło...")

def foo_command(name, args):
    ekg.echo("Wywołane polecenie foo!")

def varchange(name, newval):
    ekg.echo("Zmienna %s zmieniła wartość na %s" % (name, newval) )
    
ekg.command_bind('foo', foo_command)
ekg.handler_bind('protocol-message-received', message_handler)
ekg.handler_bind('protocol-message-sent', own_message_handler)
ekg.handler_bind('protocol-status', status_handler)
ekg.handler_bind('protocol-connected', connect_handler)
ekg.handler_bind('protocol-disconnected', disconnect_handler)
# ekg.handler_bind('ui-keypress', keypress)
ekg.variable_add('zmienna_testowa', 'wartość', varchange)
