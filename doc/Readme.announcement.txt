******************************
* Announcement Server Module *
******************************

Description:
------------

The annoucement server plays a wav file to the caller.
It searches for files to play following these rules:

 1) [announce_path]/[Domain]/[User].wav
 2) [announce_path]/[User].wav
 3) [announce_path]/[default_announce]

That's it.

It can also loop the file if configured to do so. 

Now that really is it.
