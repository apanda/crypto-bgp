import random 
import networkx
import pylab

GRAPH_SIZE = 10

def generate_graph(path):
  z = []
  for index in range(GRAPH_SIZE):
    degree = int(random.gammavariate(alpha = 3.0, beta = 2.0))
    z.append(degree)

  G = networkx.configuration_model( z )
  AG = networkx.to_agraph(G)

  for node in G.nodes(data = True):
    node[1]['node_id'] = node[0]
    node[1]['type'] = 99

  networkx.draw( G )
  networkx.write_dot(G, path)



def main():
  generate_graph("dot.dot")



if __name__ == "__main__":
  main()
