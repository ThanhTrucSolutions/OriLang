using System.Security.Cryptography;
using System.Text;

namespace Ori;

/// <summary>
/// Reads/writes the encrypted <c>.orx</c> image format.
///
/// Defense-in-depth so the bytecode is hard to reverse engineer:
///   1. The program is serialized to a private binary layout.
///   2. Every opcode byte is run through a secret 256-entry permutation,
///      so even decrypted bytes don't map to public opcode numbers.
///   3. The whole payload is encrypted with ChaCha20 under a key derived
///      from a master key embedded (split/obfuscated) in the VM + a per-file salt.
///   4. An HMAC-SHA256 tag detects any tampering; the VM refuses bad images.
///
/// Without the VM binary (which holds the master key) an .orx file is just
/// salt + nonce + random-looking ciphertext + a MAC.
/// </summary>
public static class Container
{
    private static readonly byte[] Magic = Encoding.ASCII.GetBytes("ORIX");
    private const byte Version = 1;

    private const int SaltLen = 16;
    private const int NonceLen = 12;
    private const int MacLen = 32;
    private const int HeaderLen = 4 + 1 + 1 + SaltLen + NonceLen + 4; // up to cipherLen field

    // ----- master key (obfuscated: stored as A xor B, assembled at runtime) -----
    // This is intentionally not a plain contiguous constant in the assembly.
    private static readonly byte[] _ka =
    {
        0x9E, 0x37, 0x79, 0xB9, 0x7F, 0x4A, 0x7C, 0x15,
        0xF3, 0x9C, 0xC0, 0x60, 0x5C, 0xED, 0xC8, 0x34,
        0x1B, 0x2D, 0x6A, 0xE5, 0x49, 0x0C, 0x3F, 0x71,
        0x88, 0x14, 0x57, 0xA2, 0xD3, 0x6E, 0xBC, 0x05
    };
    private static readonly byte[] _kb =
    {
        0x21, 0x66, 0x4C, 0x9A, 0x7E, 0xD5, 0x10, 0x88,
        0x44, 0x37, 0x70, 0x1B, 0xE9, 0x12, 0x0F, 0x6F,
        0xA5, 0xC3, 0x91, 0x2E, 0x77, 0xBA, 0xD0, 0x49,
        0x5B, 0xE2, 0x08, 0x3C, 0x6D, 0x9F, 0x14, 0xF0
    };

    // The effective master key is not stored anywhere — it is *reconstructed* at
    // runtime by hashing several scattered sources together (the xor of two
    // arrays, plus bytes pulled from the secret opcode permutation). This means
    // no contiguous 32-byte key constant exists in the compiled binary to grep for.
    private static byte[] MasterKey()
    {
        var seed = new byte[32 + 16 + 8];
        for (int i = 0; i < 32; i++) seed[i] = (byte)(_ka[i] ^ _kb[i]);
        for (int i = 0; i < 16; i++) seed[32 + i] = Perm[(i * 17 + 3) & 0xFF];
        BitConverter.GetBytes(PermSeed).CopyTo(seed, 48);
        BitConverter.GetBytes(0xA5C3_91E2u).CopyTo(seed, 52);
        using var sha = SHA256.Create();
        return sha.ComputeHash(seed);
    }

    // ----- operand whitening -----
    // Even after ChaCha20 is peeled off, instruction operands (const indices,
    // jump targets, slots) are XOR-masked by a deterministic keystream so the
    // decrypted byte layout still does not read as plain bytecode.
    private const uint WhiteSeed = 0x1B873593u;

    private static uint NextWhite(ref uint state)
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return state;
    }

    // ----- secret opcode permutation -----
    private const uint PermSeed = 0xC0FFEE17u;
    private static readonly byte[] Perm = BuildPerm(PermSeed);
    private static readonly byte[] InvPerm = BuildInverse(Perm);

    private static byte[] BuildPerm(uint seed)
    {
        var p = new byte[256];
        for (int i = 0; i < 256; i++) p[i] = (byte)i;
        // Deterministic Fisher-Yates with an xorshift PRNG.
        uint state = seed;
        for (int i = 255; i > 0; i--)
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            int j = (int)(state % (uint)(i + 1));
            (p[i], p[j]) = (p[j], p[i]);
        }
        return p;
    }

    private static byte[] BuildInverse(byte[] p)
    {
        var inv = new byte[256];
        for (int i = 0; i < 256; i++) inv[p[i]] = (byte)i;
        return inv;
    }

    // ----- key derivation -----
    private static byte[] Derive(byte[] master, byte[] salt, string purpose)
    {
        using var sha = SHA256.Create();
        var label = Encoding.ASCII.GetBytes(purpose);
        var buf = new byte[master.Length + salt.Length + label.Length];
        Buffer.BlockCopy(master, 0, buf, 0, master.Length);
        Buffer.BlockCopy(salt, 0, buf, master.Length, salt.Length);
        Buffer.BlockCopy(label, 0, buf, master.Length + salt.Length, label.Length);
        return sha.ComputeHash(buf);
    }

    // =========================================================
    //  PACK  (program -> encrypted .orx bytes)
    // =========================================================
    public static byte[] Pack(OriProgram prog)
    {
        byte[] plain = Serialize(prog);

        var salt = new byte[SaltLen];
        var nonce = new byte[NonceLen];
        RandomNumberGenerator.Fill(salt);
        RandomNumberGenerator.Fill(nonce);

        byte[] master = MasterKey();
        byte[] encKey = Derive(master, salt, "ORI-ENC-v1");
        byte[] macKey = Derive(master, salt, "ORI-MAC-v1");

        byte[] cipher = ChaCha20.Crypt(encKey, nonce, 1, plain);

        using var ms = new MemoryStream();
        using (var w = new BinaryWriter(ms, Encoding.UTF8, leaveOpen: true))
        {
            w.Write(Magic);
            w.Write(Version);
            w.Write((byte)0); // flags
            w.Write(salt);
            w.Write(nonce);
            w.Write(cipher.Length);
            w.Write(cipher);
        }

        byte[] body = ms.ToArray();
        byte[] mac;
        using (var hmac = new HMACSHA256(macKey))
            mac = hmac.ComputeHash(body);

        var outBuf = new byte[body.Length + MacLen];
        Buffer.BlockCopy(body, 0, outBuf, 0, body.Length);
        Buffer.BlockCopy(mac, 0, outBuf, body.Length, MacLen);
        return outBuf;
    }

    // =========================================================
    //  UNPACK  (encrypted .orx bytes -> program)
    // =========================================================
    public static OriProgram Unpack(byte[] image)
    {
        if (image.Length < HeaderLen + MacLen)
            throw new OriImageError("image too small");

        for (int i = 0; i < Magic.Length; i++)
            if (image[i] != Magic[i]) throw new OriImageError("bad magic (not an .orx image)");

        int p = 4;
        byte version = image[p++];
        if (version != Version) throw new OriImageError($"unsupported version {version}");
        p++; // flags

        var salt = new byte[SaltLen];
        Buffer.BlockCopy(image, p, salt, 0, SaltLen); p += SaltLen;
        var nonce = new byte[NonceLen];
        Buffer.BlockCopy(image, p, nonce, 0, NonceLen); p += NonceLen;

        int cipherLen = image[p] | (image[p + 1] << 8) | (image[p + 2] << 16) | (image[p + 3] << 24);
        p += 4;
        if (cipherLen < 0 || p + cipherLen + MacLen > image.Length)
            throw new OriImageError("corrupt image (length mismatch)");

        int bodyLen = p + cipherLen;

        byte[] master = MasterKey();
        byte[] macKey = Derive(master, salt, "ORI-MAC-v1");

        // Verify integrity before decrypting.
        byte[] expectedMac;
        using (var hmac = new HMACSHA256(macKey))
        {
            var body = new byte[bodyLen];
            Buffer.BlockCopy(image, 0, body, 0, bodyLen);
            expectedMac = hmac.ComputeHash(body);
        }
        for (int i = 0; i < MacLen; i++)
            if (image[bodyLen + i] != expectedMac[i])
                throw new OriImageError("integrity check failed (tampered or wrong key)");

        var cipher = new byte[cipherLen];
        Buffer.BlockCopy(image, p, cipher, 0, cipherLen);

        byte[] encKey = Derive(master, salt, "ORI-ENC-v1");
        byte[] plain = ChaCha20.Crypt(encKey, nonce, 1, cipher);

        return Deserialize(plain);
    }

    // =========================================================
    //  Serialization of the program structure
    // =========================================================
    private static byte[] Serialize(OriProgram prog)
    {
        using var ms = new MemoryStream();
        using var w = new BinaryWriter(ms, Encoding.UTF8);

        w.Write(prog.Consts.Count);
        foreach (var c in prog.Consts)
        {
            w.Write((byte)c.Type);
            switch (c.Type)
            {
                case ValueType.Number: w.Write(c.AsNumber); break;
                case ValueType.Str: WriteString(w, c.AsStr); break;
                case ValueType.Bool: w.Write((byte)(c.AsBool ? 1 : 0)); break;
                case ValueType.Nil: break;
                default: throw new OriImageError("non-serializable constant");
            }
        }

        w.Write(prog.MainIndex);
        w.Write(prog.Functions.Count);
        uint white = WhiteSeed;
        foreach (var fn in prog.Functions)
        {
            WriteString(w, fn.Name);
            w.Write(fn.Arity);
            w.Write(fn.LocalCount);
            w.Write(fn.Code.Count);
            foreach (var ins in fn.Code)
            {
                // opcode obfuscated through the secret permutation;
                // operand XOR-masked by the whitening keystream
                w.Write(Perm[(byte)ins.Op]);
                w.Write(ins.Arg ^ (int)NextWhite(ref white));
            }
        }

        w.Flush();
        return ms.ToArray();
    }

    private static OriProgram Deserialize(byte[] data)
    {
        using var ms = new MemoryStream(data);
        using var r = new BinaryReader(ms, Encoding.UTF8);

        var prog = new OriProgram();
        int constCount = r.ReadInt32();
        for (int i = 0; i < constCount; i++)
        {
            var type = (ValueType)r.ReadByte();
            switch (type)
            {
                case ValueType.Number: prog.Consts.Add(Value.Number(r.ReadDouble())); break;
                case ValueType.Str: prog.Consts.Add(Value.Str(ReadString(r))); break;
                case ValueType.Bool: prog.Consts.Add(Value.Bool(r.ReadByte() != 0)); break;
                case ValueType.Nil: prog.Consts.Add(Value.Nil); break;
                default: throw new OriImageError("bad constant tag");
            }
        }

        prog.MainIndex = r.ReadInt32();
        int fnCount = r.ReadInt32();
        uint white = WhiteSeed;
        for (int i = 0; i < fnCount; i++)
        {
            var fn = new OriFunction
            {
                Name = ReadString(r),
                Arity = r.ReadInt32(),
                LocalCount = r.ReadInt32(),
            };
            int codeCount = r.ReadInt32();
            for (int j = 0; j < codeCount; j++)
            {
                byte raw = r.ReadByte();
                var op = (OpCode)InvPerm[raw];
                int arg = r.ReadInt32() ^ (int)NextWhite(ref white);
                fn.Code.Add(new Instr(op, arg));
            }
            prog.Functions.Add(fn);
        }
        return prog;
    }

    private static void WriteString(BinaryWriter w, string s)
    {
        var bytes = Encoding.UTF8.GetBytes(s ?? "");
        w.Write(bytes.Length);
        w.Write(bytes);
    }

    private static string ReadString(BinaryReader r)
    {
        int len = r.ReadInt32();
        var bytes = r.ReadBytes(len);
        return Encoding.UTF8.GetString(bytes);
    }
}
