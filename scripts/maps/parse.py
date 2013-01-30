#!/usr/bin/python

from collections import deque
import sys

import heapq

class MyPriQueue(object):
    def __init__(self):
        self.heap = []

    def add(self, d, pri):
        heapq.heappush(self.heap, (pri, d))

    def get(self):
        pri, d = heapq.heappop(self.heap)
        return (pri, d)


GRAPH_SIZE = 5976
BUCKET_SIZE = 2

buckets = deque()
q = MyPriQueue()
relation = {}
relMap = {}

relMap['p2c'] = 2
relMap['p2p'] = 1
relMap['c2p'] = 0


for i in range(BUCKET_SIZE):
  buckets.append( [] )
  q.add( [], 0 )



def main():
  inspect()



def inspect():
  edges = parse()

  nodeMapping = {}
  degrees = {}
  for (src, dst, rel) in edges:

    if src not in degrees: degrees[src] = 0
    degrees[src] = degrees[src] + 1

    pair = (src, dst)
    relation[ (src, dst) ] = relMap[rel]



  degreeRanking = []
  for (vertex, degree) in degrees.items():
    degreeRanking.append( (degree, vertex) )

  degreeRanking.sort(key = lambda (degree, vertex): degree, reverse = True)

  top = degreeRanking[0]

  counter = 0
  for (ranking, vertex) in degreeRanking:

    if counter >= (GRAPH_SIZE):
      assert ranking == 1
      continue

    (pri, d) = q.get()
    pri = pri + degrees[vertex]
    d.append(vertex)
    q.add(d, pri)

    bucket = buckets.popleft()
    bucket.append(vertex)
    buckets.append(bucket)

    counter = counter + 1


  buckets2 = []
  while(True):
    try:
      (pri, d) = q.get()
      buckets2.append(d)
    except: break


  for bucket in buckets2:
      t = 0
      for vertex in bucket:
        t = t + degrees[vertex]


  reverseMapping = {}
  bucketList = []
  for bucket in buckets2:
    bucketList = bucketList + bucket

  counter = 0
  for vertex in bucketList:

    if counter > (GRAPH_SIZE):
      break

    reverseMapping[counter] = vertex
    nodeMapping[vertex] = counter
    counter = counter + 1

  '''
  lines = parse('sorted')
  for tokens in lines:
    vertex = int( tokens[4][:-1] )
    count = int( tokens[5] )
    print reverseMapping[vertex], count
  '''

  for i in range( GRAPH_SIZE ):
    nodeID = '[node_id=%d]' %(i)
    line = '%d %s;' %(i, nodeID)
    #print line

  uniqueEdges = set()
  for (src, dst, rel) in edges:
    try:
      src = nodeMapping[src]
      dst = nodeMapping[dst]
    except: continue
    pair = [src, dst]
    pair.sort()
    revSrc = reverseMapping[src]
    revDst = reverseMapping[dst]
    rel = relation[ (revSrc, revDst) ]
    uniqueEdges.add( tuple(pair + [rel]) )

  nnn = list( uniqueEdges )
  nnn.sort(key = lambda (src, dst, rel): rel)
  nnn.sort(key = lambda (src, dst, rel): src)

  for (src, dst, rel) in nnn:
    line = '%s %s %s' %(src, dst, rel)
    print line



def dot():
  edges = parse()
  print 'graph G {'
  for edge in edges:
    print edge[0], '--', edge[1], ';'
  print '}'


def parse(filename = 'map.20120101_nonStub'):
  with open(filename) as f:
    lines = f.readlines()
  
  parsed = map(lambda line: line.split(), lines)
  return parsed

main()
