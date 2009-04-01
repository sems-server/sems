Actions: 
 utils.getNewId(string varname)
 -- play count for laguages that have single digits after the 10s (like english)
 utils.playCountRight(int cnt [, string basedir]) 
 -- play count for laguages that have single digits befire the 10s (like german)  
 utils.playCountLeft(int cnt [, string basedir])


 utils.spell(string word[, string basedir])
  plays each character in the word (e.g. utils.spell(321,wav/digits/) plays
    wav/digits/3.wav, wav/digits/2.wav, wav/digits/1.wav 
  like SayDigits 

SCUSpellAction

Conditions: 
