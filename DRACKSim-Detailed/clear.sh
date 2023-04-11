ipcs | nawk -v u=`whoami` '/Shared/,/^$/{ if($6==0&&$3==u) print "ipcrm shm",$2,";"}/Semaphore/,/^$/{ if($3==u) print "ipcrm sem",$2,";"}' | /bin/sh
