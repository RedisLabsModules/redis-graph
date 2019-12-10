#
# Copyright (c) 2015-2019 "Neo Technology,"
# Network Engine for Objects in Lund AB [http://neotechnology.com]
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Attribution Notice under the terms of the Apache License 2.0
#
# This work was created by the collective efforts of the openCypher community.
# Without limiting the terms of Section 6, any Derivative Work that is not
# approved by the public consensus process of the openCypher Implementers Group
# should not be described as “Cypher” (and Cypher® is a registered trademark of
# Neo4j Inc.) or as "openCypher". Extensions by implementers or prototypes or
# proposals for change that have been documented or implemented should only be
# described as "implementation extensions to Cypher" or as "proposed changes to
# Cypher that are not yet approved by the openCypher community".
#

#encoding: utf-8

Feature: AggregationAcceptance

    Scenario: Support multiple divisions in aggregate function
        Given an empty graph
        And having executed:
            """
            UNWIND range(0, 7250) AS i
            CREATE ()
            """
        When executing query:
            """
            MATCH (n)
            WITH count(n) / 60 / 60 AS count
            RETURN floor(count)
            """
        Then the result should be:
            | count |
            | 2     |
        And no side effects

    Scenario: Support column renaming for aggregates as well
        Given an empty graph
        And having executed:
            """
            UNWIND range(0, 10) AS i
            CREATE ()
            """
        When executing query:
            """
            MATCH ()
            RETURN count(*) AS columnName
            """
        Then the result should be:
            | columnName |
            | 11         |
        And no side effects

    Scenario: Aggregates inside normal functions
        Given an empty graph
        And having executed:
            """
            UNWIND range(0, 10) AS i
            CREATE ()
            """
        When executing query:
            """
            MATCH (a)
            RETURN size(collect(a))
            """
        Then the result should be:
            | size(collect(a)) |
            | 11               |
        And no side effects

    @skip
    Scenario: Handle aggregates inside non-aggregate expressions
        Given an empty graph
        When executing query:
            """
            MATCH (a {name: 'Andres'})<-[:FATHER]-(child)
            RETURN {foo: a.name='Andres', kids: collect(child.name)}
            """
        Then the result should be:
            | {foo: a.name='Andres', kids: collect(child.name)} |
        And no side effects

    Scenario: Count nodes
        Given an empty graph
        And having executed:
            """
            CREATE (a:L), (b1), (b2)
            CREATE (a)-[:A]->(b1), (a)-[:A]->(b2)
            """
        When executing query:
            """
            MATCH (a:L)-[rel]->(b)
            RETURN a, count(*)
            """
        Then the result should be:
            | a    | count(*) |
            | (:L) | 2        |
        And no side effects

    @skip
    Scenario: Sort on aggregate function and normal property
        Given an empty graph
        And having executed:
            """
            CREATE ({division: 'Sweden'})
            CREATE ({division: 'Germany'})
            CREATE ({division: 'England'})
            CREATE ({division: 'Sweden'})
            """
        When executing query:
            """
            MATCH (n)
            RETURN n.division, count(*)
            ORDER BY count(*) DESC, n.division ASC
            """
        Then the result should be, in order:
            | n.division | count(*) |
            | 'Sweden'   | 2        |
            | 'England'  | 1        |
            | 'Germany'  | 1        |
        And no side effects

    Scenario: Aggregate on property
        Given an empty graph
        And having executed:
            """
            CREATE ({num: 33})
            CREATE ({num: 33})
            CREATE ({num: 42})
            """
        When executing query:
            """
            MATCH (n)
            RETURN n.num, count(*)
            """
        Then the result should be:
            | n.num | count(*) |
            | 42    | 1        |
            | 33    | 2        |
        And no side effects

    Scenario: Count non-null values
        Given an empty graph
        And having executed:
            """
            CREATE ({name: 'a', num: 33})
            CREATE ({name: 'a'})
            CREATE ({name: 'b', num: 42})
            """
        When executing query:
            """
            MATCH (n)
            RETURN n.name, count(n.num)
            """
        Then the result should be:
            | n.name | count(n.num) |
            | 'a'    | 1            |
            | 'b'    | 1            |
        And no side effects

    Scenario: Sum non-null values
        Given an empty graph
        And having executed:
            """
            CREATE ({name: 'a', num: 33})
            CREATE ({name: 'a'})
            CREATE ({name: 'a', num: 42})
            """
        When executing query:
            """
            MATCH (n)
            RETURN n.name, sum(n.num)
            """
        Then the result should be:
            | n.name | sum(n.num) |
            | 'a'    | 75         |
        And no side effects

    Scenario: Handle aggregation on functions
        Given an empty graph
        And having executed:
            """
            CREATE (a:L), (b1), (b2)
            CREATE (a)-[:A]->(b1), (a)-[:A]->(b2)
            """
        When executing query:
            """
            MATCH p=(a:L)-[*]->(b)
            RETURN b, avg(length(p))
            """
        Then the result should be:
            | b  | avg(length(p)) |
            | () | 1.0            |
            | () | 1.0            |
        And no side effects

    @skip
    Scenario: Distinct on unbound node
        Given an empty graph
        When executing query:
            """
            OPTIONAL MATCH (a)
            RETURN count(DISTINCT a)
            """
        Then the result should be:
            | count(DISTINCT a) |
            | 0                 |
        And no side effects

    Scenario: Distinct on null
        Given an empty graph
        And having executed:
            """
            CREATE ()
            """
        When executing query:
            """
            MATCH (a)
            RETURN count(DISTINCT a.name)
            """
        Then the result should be:
            | count(DISTINCT a.name) |
            | 0                      |
        And no side effects

    
    Scenario: Collect distinct nulls
        Given any graph
        When executing query:
            """
            UNWIND [null, null] AS x
            RETURN collect(DISTINCT x) AS c
            """
        Then the result should be:
            | c  |
            | [] |
        And no side effects


    Scenario: Collect distinct values mixed with nulls
        Given any graph
        When executing query:
            """
            UNWIND [null, 1, null] AS x
            RETURN collect(DISTINCT x) AS c
            """
        Then the result should be:
            | c   |
            | [1] |
        And no side effects

    Scenario: Aggregate on list values
        Given an empty graph
        And having executed:
            """
            CREATE ({color: ['red']})
            CREATE ({color: ['blue']})
            CREATE ({color: ['red']})
            """
        When executing query:
            """
            MATCH (a)
            RETURN DISTINCT a.color, count(*)
            """
        Then the result should be:
            | a.color  | count(*) |
            | ['red']  | 2        |
            | ['blue'] | 1        |
        And no side effects

    Scenario: Aggregates in aggregates
        Given any graph
        When executing query:
            """
            RETURN count(count(*))
            """
        Then a SyntaxError should be raised at compile time: NestedAggregation

    Scenario: Aggregates with arithmetics
        Given an empty graph
        And having executed:
            """
            CREATE ()
            """
        When executing query:
            """
            MATCH ()
            RETURN count(*) * 10 AS c
            """
        Then the result should be:
            | c  |
            | 10 |
        And no side effects

    Scenario: Aggregates ordered by arithmetics
        Given an empty graph
        And having executed:
            """
            CREATE (:A), (:X), (:X)
            """
        When executing query:
            """
            MATCH (a:A), (b:X)
            RETURN count(a) * 10 + count(b) * 5 AS x
            ORDER BY x
            """
        Then the result should be, in order:
            | x  |
            | 30 |
        And no side effects


    Scenario: Multiple aggregates on same variable
        Given an empty graph
        And having executed:
            """
            CREATE ()
            """
        When executing query:
            """
            MATCH (n)
            RETURN count(n), collect(n)
            """
        Then the result should be:
            | count(n) | collect(n) |
            | 1        | [()]       |
        And no side effects

    Scenario: Simple counting of nodes
        Given an empty graph
        And having executed:
            """
            UNWIND range(1, 100) AS i
            CREATE ()
            """
        When executing query:
            """
            MATCH ()
            RETURN count(*)
            """
        Then the result should be:
            | count(*) |
            | 100      |
        And no side effects

    Scenario: Aggregation of named paths
        Given an empty graph
        And having executed:
            """
            CREATE (a:A), (b:B), (c:C), (d:D), (e:E), (f:F)
            CREATE (a)-[:R]->(b)
            CREATE (c)-[:R]->(d)
            CREATE (d)-[:R]->(e)
            CREATE (e)-[:R]->(f)
            """
        When executing query:
            """
            MATCH p = (a)-[*]->(b)
            RETURN collect(nodes(p)) AS paths, length(p) AS l
            ORDER BY l
            """
        Then the result should be, in order:
            | paths                                                    | l |
            | [[(:A), (:B)], [(:C), (:D)], [(:D), (:E)], [(:E), (:F)]] | 1 |
            | [[(:C), (:D), (:E)], [(:D), (:E), (:F)]]                 | 2 |
            | [[(:C), (:D), (:E), (:F)]]                               | 3 |
        And no side effects

    Scenario: Aggregation with `min()`
        Given an empty graph
        And having executed:
            """
            CREATE (a:T {name: 'a'}), (b:T {name: 'b'}), (c:T {name: 'c'})
            CREATE (a)-[:R]->(b)
            CREATE (a)-[:R]->(c)
            CREATE (c)-[:R]->(b)
            """
        When executing query:
            """
            MATCH p = (a:T {name: 'a'})-[:R*]->(other:T)
            WHERE other <> a
            WITH a, other, min(length(p)) AS len
            RETURN a.name AS name, collect(other.name) AS others, len
            """
        Then the result should be:
            | name | others     | len |
            | 'a'  | ['b', 'c'] | 1   |
        And no side effects

    @skip
    Scenario: Handle subexpression in aggregation also occurring as standalone expression with nested aggregation in a literal map
        Given an empty graph
        And having executed:
            """
            CREATE (:A), (:B {num: 42})
            """
        When executing query:
            """
            MATCH (a:A), (b:B)
            RETURN coalesce(a.num, b.num) AS foo,
            b.num AS bar,
            {name: count(b)} AS baz
            """
        Then the result should be:
            | foo | bar | baz      |
            | 42  | 42  | {name:1} |
        And no side effects

@crash
    @skip
    Scenario: Projection during aggregation in WITH before MERGE and after WITH with predicate
        Given an empty graph
        And having executed:
            """
            CREATE (:A {num: 42})
            """
        When executing query:
            """
            UNWIND [42] AS props
            WITH props WHERE props > 32
            WITH DISTINCT props AS p
            MERGE (a:A {num: p})
            RETURN a.num AS prop
            """
        Then the result should be:
            | prop |
            | 42   |
        And no side effects

    Scenario: No overflow during summation
        Given any graph
        When executing query:
            """
            UNWIND range(1000000, 2000000) AS i
            WITH i
            LIMIT 3000
            RETURN sum(i)
            """
        Then the result should be:
            | sum(i)     |
            | 3004498500 |
        And no side effects

    @skip
    Scenario: Counting with loops
        Given an empty graph
        And having executed:
            """
            CREATE (a), (a)-[:R]->(a)
            """
        When executing query:
            """
            MATCH ()-[r]-()
            RETURN count(r)
            """
        Then the result should be:
            | count(r) |
            | 1        |
        And no side effects

    Scenario: `max()` should aggregate strings
        Given any graph
        When executing query:
            """
            UNWIND ['a', 'b', 'B', null, 'abc', 'abc1'] AS i
            RETURN max(i)
            """
        Then the result should be:
            | max(i) |
            | 'b'    |
        And no side effects

    Scenario: `min()` should aggregate strings
        Given any graph
        When executing query:
            """
            UNWIND ['a', 'b', 'B', null, 'abc', 'abc1'] AS i
            RETURN min(i)
            """
        Then the result should be:
            | min(i) |
            | 'B'    |
        And no side effects
