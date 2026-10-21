# Minimal pycryptodome-compatible AES on top of `cryptography`.
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
MODE_ECB, MODE_CBC, MODE_CTR = 1, 2, 6
class _C:
    def __init__(self, key, mode, iv=None, nonce=None, initial_value=None):
        if mode == MODE_ECB: m = modes.ECB()
        elif mode == MODE_CBC: m = modes.CBC(iv)
        elif mode == MODE_CTR:
            ctr = initial_value if isinstance(initial_value, (bytes, bytearray)) else int(initial_value or 0).to_bytes(16, 'big')
            m = modes.CTR((nonce or b'') + ctr)
        self.c = Cipher(algorithms.AES(key), m)
    def decrypt(self, d): x = self.c.decryptor(); return x.update(d) + x.finalize()
    def encrypt(self, d): x = self.c.encryptor(); return x.update(d) + x.finalize()
def new(key, mode, iv=None, nonce=None, initial_value=None, **kw):
    return _C(key, mode, iv, nonce, initial_value)
