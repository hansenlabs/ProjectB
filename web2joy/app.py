from flask import Flask, render_template, request
from flask_sock import Sock
from evdev import UInput, AbsInfo
from evdev import ecodes as e 

app = Flask(__name__)
sock = Sock(app)
jdlist = list()
clientdict={}


next_index=0
def get_index(name):
    global next_index
    global clientdict
    global jdlist
    capabilities = {
     e.EV_KEY : [e.BTN_A, e.BTN_B],
     e.EV_ABS : [
          (e.ABS_X, AbsInfo(value=0, min=-32767, max=32767, fuzz=0, flat=0, resolution=13)),
          (e.ABS_Y, AbsInfo(0, -32767, 32767, 0, 0, 13))]
    }
    return_index=0
    if name in clientdict:
      return_index=clientdict[name]
    else:
      return_index=256*next_index
      clientdict[name]=next_index
      next_index=next_index+1
      jdlist.append(UInput(capabilities,name+"_Web2Joy"))
    return return_index

@app.route('/stick', methods=['GET', 'POST'])
def stick():
    if request.method == 'POST':
        entry = request.form['Name']
        print(entry)
        result = get_index(entry)
        print(result)
        return render_template('index.html', result=result)
    else:   
        return render_template('index.html')

@sock.route('/echo')
def echo(sock):
    global jdlist
    while True:
        data = sock.receive()
        #print(data)
        #print("hab ich empfangen")
        #get joystick ID
        idata=int(data)
        jid=int(idata/256)
        jval=idata%256
        jfd=jdlist[jid]
        yval=((jval & 1)>0) *-32767 + ((jval & 2)>0)*32767
        jfd.write(e.EV_ABS, e.ABS_Y, yval)
        #jfd.syn() 
        xval=((jval & 4)>0) *-32767 + ((jval & 8)>0)*32767
        jfd.write(e.EV_ABS, e.ABS_X, xval)
        #jfd.syn() 
        jfd.write(e.EV_KEY, e.BTN_A, ((jval & 16)>0)) #1 for btn press
        #jfd.syn()
        jfd.write(e.EV_KEY, e.BTN_B, ((jval & 32)>0)) #1 for btn press
        jfd.syn()                
        #print(data)
        #sock.send(data)

if __name__ == "__main__":
    app.run(host='0.0.0.0')
