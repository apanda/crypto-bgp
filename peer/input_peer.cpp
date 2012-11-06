#include <peer/input_peer.hpp>

#include <gsl/gsl_interp.h>
#include <gsl/gsl_poly.h>

Input_peer::Input_peer() : Peer(12122)  {}
