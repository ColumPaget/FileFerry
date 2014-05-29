fileferry -l /var/log/Backup.log ssh:axiom@server.parcelroute.site -p Greenheart -c "lmkdir /home/Backups; lcd /home/Backups; cd /home ;  get -s -r -x *.o,*.cgi,*.exe,*.so,*.a Software; cd Site; lmkdir Site; lcd Site; get -s -r Settings StaticData"

