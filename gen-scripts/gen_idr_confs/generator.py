import sys
import subprocess
if len(sys.argv) < 3:
    print str.format("{0} domains base_port", sys.argv[0])
    sys.exit(1)
domains = int(sys.argv[1])
base_port = long(sys.argv[2])
peer_conf_file = open('config.peer01.properties')
peer_conf = peer_conf_file.read()

ppeer1_conf_file = open('config.privacypeer01.properties')
ppeer1_conf = ppeer1_conf_file.read()
ppeer2_conf_file = open('config.privacypeer02.properties')
ppeer2_conf = ppeer2_conf_file.read()
ppeer3_conf_file = open('config.privacypeer03.properties')
ppeer3_conf = ppeer3_conf_file.read()

for n in xrange(1, domains + 1):
    subprocess.call(str.format("cp -R IDR IDR{0}", n), shell=True)
    args = {'node_id': n, 'pp1_port':base_port, 'pp2_port':base_port+1, 'pp3_port':base_port+2, 'node_count':domains}
    base_port = base_port + 3
    peer1_file = open(str.format('IDR{0}/peer01/config.peer01.properties', n), 'w') 
    peer1_file.write(str.format(peer_conf, **args))
    peer1_file.close()
    
    for i in xrange(1, 4):
        ppeer_file = open(str.format('IDR{0}/privacypeer0{1}/config.privacypeer0{1}.properties', n, i), 'w')
        ppeer_file.write(str.format(eval(str.format('ppeer{0}_conf', i)), **args))
        ppeer_file.close()
 
