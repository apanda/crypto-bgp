import random 
import networkx
import pylab

GRAPH_SIZE = 100

def generate_graph(path):
  z = []
  for index in range(GRAPH_SIZE):
    degree = int(random.gammavariate(alpha = 3.0, beta = 2.0))
    z.append(degree)

  print(z)

  G = networkx.configuration_model( z )
  G = networkx.Graph(G)
  G.remove_edges_from(G.selfloop_edges())

  for node in G.nodes(data = True):
    node[1]['node_id'] = node[0]

  networkx.draw( G )
  networkx.write_dot(G, path)



def main():
  generate_graph("dot.dot")



if __name__ == "__main__":
  main()
