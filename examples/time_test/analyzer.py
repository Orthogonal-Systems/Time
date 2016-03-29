import serial
#import time
import subprocess

ser = serial.Serial('/dev/ttyACM0',115200)
ready = False
while True:
    try:
        line = ser.readline()

        #ts = float(time.time())
        ts = subprocess.Popen(["date","+%s.%N"],stdout=subprocess.PIPE).communicate()[0]
        [ts_sec, ts_ns] = ts.split(".",1)
        ts_sec = long(ts_sec)*(2**32)
        ts_fracSec = (long(ts_ns) * (2**32))/1000000000
        ts = ts_sec + ts_fracSec

        arduino_time = long(line,16)

        arduino_seconds = arduino_time
        server_seconds = ts
        delta_sec =  float(server_seconds - arduino_seconds)/(2**32)
        print delta_sec
        if ready:
            with open("delta_t.txt","a") as f:
                f.write(str(delta_sec)+",")

    except ValueError:
        if line[:8] == "uC drift":
            print line.strip()
            try:
                dc = float(line.rsplit(" ",1)[1])/(2**31)
                print "drift: {}".format(dc)
                if ready:
                    with open("drift_corrections.txt","a") as f:
                        f.write(str(dc)+"\n")
                    with open("delta_t.txt","a") as f:
                        f.write("\n")
            except ValueError:
                pass
        else:
            print "not a hex string"
        ready = True
