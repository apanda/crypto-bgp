#!/bin/bash
java -cp "../../lib/sepia.jar:../../lib/mpc.jar" -Djavax.net.ssl.trustStore=privacypeer02KeyStore.jks  MainCmd  -p 1 -c config.privacypeer02.properties
