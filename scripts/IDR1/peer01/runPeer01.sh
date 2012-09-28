#!/bin/bash
java -cp "../../lib/sepia.jar:../../lib/mpc.jar:../test.txt" -Djavax.net.ssl.trustStore=peer01KeyStore.jks  MainCmd -p 0 -c config.peer01.properties
#java -Xdebug -Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=10044 -cp "../../sepia.jar:../../mpc.jar" -Djavax.net.ssl.trustStore=peer01KeyStore.jks  MainCmd -v -p 0 -c config.peer01.properties
