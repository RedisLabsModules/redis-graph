import os
import sys
import string
import threading
from RLTest import Env
from redisgraph import Graph

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from base import FlowTestsBase

GRAPH_ID = "index"
GRAPH_ID_2 = "index2"
GRAPH_ID_3 = "index3"
GRAPH_ID_4 = "index4"
GRAPH_ID_5 = "index5"
GRAPH_ID_6 = "index6"
redis_graph = None
redis_graph2 = None
redis_graph3 = None
redis_graph4 = None
redis_graph5 = None
redis_graph6 = None

# run n_iterations and create and remove index in each iteration
def index_operations(graph, n_iterations):
    for i in range(n_iterations):
        graph.query("CREATE INDEX ON :person(age, name)")
        graph.query("DROP INDEX ON :person(age, name)")

class testIndexCreationFlow(FlowTestsBase):
    def __init__(self):
        self.env = Env(decodeResponses=True)
        global redis_graph
        global redis_graph2
        global redis_graph3
        global redis_graph4
        global redis_graph5
        global redis_graph6
        redis_con = self.env.getConnection()
        redis_graph = Graph(GRAPH_ID, redis_con)
        redis_graph2 = Graph(GRAPH_ID_2, redis_con)
        redis_graph3 = Graph(GRAPH_ID_3, redis_con)
        redis_graph4 = Graph(GRAPH_ID_4, redis_con)
        redis_graph5 = Graph(GRAPH_ID_5, redis_con)
        redis_graph6 = Graph(GRAPH_ID_6, redis_con)


    # full-text index creation
    def test01_fulltext_index_creation(self):
        # create an index over L:v0
        result = redis_graph.query("CALL db.idx.fulltext.createNodeIndex('L', 'v0')")
        self.env.assertEquals(result.indices_created, 1)

        # create an index over L:v1
        result = redis_graph.query("CALL db.idx.fulltext.createNodeIndex('L', 'v1')")
        self.env.assertEquals(result.indices_created, 1)

        # create an index over L:v1 and L:v2
        result = redis_graph.query("CALL db.idx.fulltext.createNodeIndex('L', 'v1', 'v2')")
        self.env.assertEquals(result.indices_created, 1)

        # create an index over L:v0, L:v1 and L:v2
        result = redis_graph.query("CALL db.idx.fulltext.createNodeIndex('L', 'v0', 'v1', 'v2')")
        self.env.assertEquals(result.indices_created, 0)

        # create an index over L:v2, L:v1 and L:v0
        result = redis_graph.query("CALL db.idx.fulltext.createNodeIndex('L', 'v2', 'v1', 'v0')")
        self.env.assertEquals(result.indices_created, 0)

        # create an index over L:v3 and L:v4
        result = redis_graph.query("CALL db.idx.fulltext.createNodeIndex('L', 'v3', 'v4')")
        self.env.assertEquals(result.indices_created, 2)

    def test02_multi_prop_index_creation(self):
        # create an index over person:age and person:name
        result = redis_graph.query("CREATE INDEX ON :person(age, name)")
        self.env.assertEquals(result.indices_created, 2)

        # try to create an index over person:age and person:name, index shouldn't be created as it already exist
        result = redis_graph.query("CREATE INDEX ON :person(age, name)")
        self.env.assertEquals(result.indices_created, 0)

        # try to create an index over person:age and person:name and person:height, index for height should be created as the rest already exist
        result = redis_graph.query("CREATE INDEX ON :person(age, age, name, height)")
        self.env.assertEquals(result.indices_created, 1)

        # try to create an index over person:gender and person:name and person:height, index for gender should be created as the rest already exist
        result = redis_graph.query("CREATE INDEX ON :person(gender, gender, name, height)")
        self.env.assertEquals(result.indices_created, 1)

    def test03_concurrent_index_creation(self):
        n_iterations = 20

        t1 = threading.Thread(target=index_operations, args=(redis_graph, n_iterations))
        t1.setDaemon(True)

        t2 = threading.Thread(target=index_operations, args=(redis_graph2, n_iterations))
        t2.setDaemon(True)

        t3 = threading.Thread(target=index_operations, args=(redis_graph3, n_iterations))
        t3.setDaemon(True)

        t4 = threading.Thread(target=index_operations, args=(redis_graph4, n_iterations))
        t4.setDaemon(True)

        t5 = threading.Thread(target=index_operations, args=(redis_graph5, n_iterations))
        t5.setDaemon(True)

        t6 = threading.Thread(target=index_operations, args=(redis_graph6, n_iterations))
        t6.setDaemon(True)

        t1.start()
        t2.start()
        t3.start()
        t4.start()
        t5.start()
        t6.start()

        # wait for threads to join
        t1.join()
        t2.join()
        t3.join()
        t4.join()
        t5.join()
        t6.join()


