from RLTest import Env
import multiprocessing as mp

con = None

def init_process_local_connection(env):
    global con
    con = env.getConnection()

class Multiproc:

    # create and init n_procs workers and their connections which will execute fn(args)
    def __init__(self, env, n_procs):

        # Forcefully setting the ctx to fork so it will be possible to pass env in initargs on macOS
        self.ctx = mp.get_context('fork')

        # creating connetions on pool init for increasing the fn calls conccurency
        # we are passing the env and not the connection so that a new connection will be created for each process
        self.pool = self.ctx.Pool(n_procs, initializer=init_process_local_connection, initargs=(env,))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.pool.terminate()

    # get a fn to run async and it args and returns AsyncResult
    def run_async(self, fn, args=tuple()):
        return self.pool.apply_async(fn, args=args)

    # executes fns asynchronously, 
    # waits for all functions to finish before collecting return values into an array
    def add_work_wait(self, fns=[], args=[tuple()]):
        assert len(fns) == len(args) and len(fns) != 0
        async_calls = [self.run_async(fn, args_i) for fn, args_i in zip(fns, args)]
        return [res.get() for res in async_calls]

# create and init n_procs workers and their connections which will execute fn(args)
def run_multiproc(env, n_procs, fn, args=tuple()):
        with Multiproc(env, n_procs) as mp:
            return mp.add_work_wait([fn]*n_procs, [args]*n_procs)