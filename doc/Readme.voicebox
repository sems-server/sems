Voicebox application
--------------------

This application implements a Voicebox: Users can call in to listen to their
messages. Voicebox is multi-domain capable, and it has a domain and language
configurable prompt set, (optionally) tells how many new and saved messages 
are in the voicebox and lets the user save and delete messages. 

Voicebox uses msg_storage plugin to list, retrieve and delete messages. The 
voicemail application can record the messages for the voicebox.

Voicebox can be made to ask for a PIN to access it, e.g. if the caller is 
not authenticated.

Voicebox invocation is controlled by application parameters (App-Param header).


application parameters (App-Param  header)
------------------------------------------

 Long     Short     Description
 --------+---------+-----------
 User     usr       user name 
 PIN      pin       PIN (will be asked for PIN if not empty)
 Domain   dom       Domain (for prompts/storage)
 Language lng       Language (for prompts/storage)
 UserID   uid       optional, overrides User above
 DomainID did       optional, overrides Domain above

Prompts
-------
Sample prompts were TTSed at spokentext.net from the text below with 
Dave's voice. 
An additional set is available here http://ftp.iptel.org/pub/sems/prompts/
You are very welcome to contribute professionally recorded prompts - just 
contact sems@iptel.org 

English prompt set (example)
----------------------------
You have -
new messages -
and -
saved messages -
in your voicebox.
First new message:
First saved message:
Press one to repeat the message, two to save the message, and three to delete the message.
Press one to repeat the message, two to save the message, three to delete the message, and four to start over.
Message saved.
Message deleted.
Next new message:
Next saved message:
There are no more messages in your voicebox. Press four to start over.
There are no messages in your voicebox. Good Bye.
Please enter your PIN.
Good bye.
one saved message -
one new message - 
zero - 
one - 
two -
three -
four -
five -
six -
seven -
eight -
nine -
ten -
eleven -
twelve -
thirteen -
fourteen -
fifteen -
sixteen -
seventeen -
eighteen -
nineteen -
twenty -
thirty -
forty -
fifty - 
sixty - 
seventy - 
eighty - 
ninety -

German prompt set
-----------------
Sie haben -
neue Nachrichten -
und -
gespeicherte Nachrichten -
in Ihrer Mailbox.
Erste neue Nachricht:
Erste gespeicherte Nachricht:
Drücken Sie eins, um die Nachricht zu wiederholen, zwei, um die Nachricht zu speichern, und drei um die Nachricht zu löschen.
Drücken Sie eins, um die Nachricht zu wiederholen, zwei, um die Nachricht zu speichern, drei um die Nachricht zu löschen, und vier um von Vorne zu beginnen.
Nachricht gespeichert.
Nachricht gelöscht.
Nächste neue Nachricht:
Nächste gespeicherte Nachricht:
Es sind keine weiteren Nachrichten in Ihrer Mailbox vorhanden. Drücken Sie vier, um von vorne zu beginnen.
Es sind keine Nachrichten in Ihrer Mailbox. Auf Wiederhören.
Bitte geben Sie ihre PIN ein.
Auf Wiederhören.
Eine gespeicherte Nachricht -
Eine neue Nachricht -
null -
eine -
zwei -
drei -
vier -
fünf -
sechs -
sieben -
acht -
neun -
zehn -
elf -
zwölf -
dreizehn -
vierzehn -
fünfzehn -
sechzehn -
siebzehn -
achtzehn -
neunzehn -
zwanzig -
dreissig -
vierzig -
fünfzig -
sechzig -
siebzig -
achtzig -
neunzig -
einund -
zweiund -
dreiund -
vierund -
fünfund -
sechsund -
siebenund -
achtund -
neunund -
