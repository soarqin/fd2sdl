"""私有被动WGC采样：复用冻结wait_state，仅记录既有帧并交替轮询。"""
import hashlib
import time
from types import FunctionType


class _PollClock:
    def __init__(self, clock):
        self.clock = clock
        self.count = 0

    def monotonic(self):
        return self.clock.monotonic()

    def sleep(self, requested):
        # 原wait_state只有80ms轮询；不更改其deadline或额外调用capture。
        if requested != 0.08:
            raise AssertionError('unexpected frozen wait_state sleep')
        interval = (0.080, 0.113)[self.count % 2]
        self.count += 1
        self.clock.sleep(interval)


class _ObservedCapture:
    def __init__(self, capture, name, samples, classify, clock):
        self.capture, self.name = capture, name
        self.samples, self.classify, self.clock = samples, classify, clock
        self.count = 0

    def __getattr__(self, key):
        return getattr(self.capture, key)

    def frame(self):
        self.count += 1
        row = dict(wait=self.name, sample=self.count,
                   capture_before=self.clock.monotonic())
        self.samples.append(row)
        try:
            frame = self.capture.frame()  # 唯一一次原WGC调用。
        except BaseException as error:
            row['capture_after'] = self.clock.monotonic()
            row['capture_error'] = dict(type=type(error).__name__, message=str(error))
            raise
        row['capture_after'] = self.clock.monotonic()
        path = self.capture.artifacts / f'{self.name}-sample-{self.count:04d}.png'
        frame.save(path)  # 原实际整帧，不拼图、不归一颜色、不再取一帧。
        row['image'] = dict(path=str(path.resolve()), size=path.stat().st_size,
                            sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        if self.classify is not None:
            row['phase_resources'] = self.classify(frame)
        return frame


def wait_state(module, capture, name, predicate, report, samples, *,
               classify=None, clock=time, **kwargs):
    """原predicate/期限/瞬态/错误路径不变；不修改共享time或producer模块。"""
    original = module.wait_state
    namespace = dict(original.__globals__, time=_PollClock(clock))
    wait = FunctionType(original.__code__, namespace, original.__name__,
                        original.__defaults__, original.__closure__)
    wait.__kwdefaults__ = original.__kwdefaults__
    return wait(_ObservedCapture(capture, name, samples, classify, clock),
                name, predicate, report, **kwargs)
