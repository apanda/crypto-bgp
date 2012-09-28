#!/bin/bash
java -cp "../../lib/sepia.jar:../../lib/mpc.jar" -Djavax.net.ssl.trustStore=privacypeer03KeyStore.jks  MainCmd -p 1 -c config.privacypeer03.properties
