namespace Ori;

/// <summary>
/// A from-scratch ChaCha20 stream cipher (RFC 8439, 20 rounds).
/// Used to encrypt the serialized bytecode inside an .orx image so the file
/// is opaque without the VM's embedded key material.
/// </summary>
internal static class ChaCha20
{
    private const int Rounds = 20;

    public static byte[] Crypt(byte[] key32, byte[] nonce12, uint counter, byte[] data)
    {
        if (key32.Length != 32) throw new ArgumentException("key must be 32 bytes");
        if (nonce12.Length != 12) throw new ArgumentException("nonce must be 12 bytes");

        var output = new byte[data.Length];
        var keystream = new byte[64];
        var state = new uint[16];

        // Constant "expand 32-byte k"
        state[0] = 0x61707865;
        state[1] = 0x3320646e;
        state[2] = 0x79622d32;
        state[3] = 0x6b206574;
        for (int i = 0; i < 8; i++)
            state[4 + i] = ReadU32(key32, i * 4);
        state[12] = counter;
        state[13] = ReadU32(nonce12, 0);
        state[14] = ReadU32(nonce12, 4);
        state[15] = ReadU32(nonce12, 8);

        int offset = 0;
        while (offset < data.Length)
        {
            Block(state, keystream);
            state[12]++; // increment block counter
            int chunk = Math.Min(64, data.Length - offset);
            for (int i = 0; i < chunk; i++)
                output[offset + i] = (byte)(data[offset + i] ^ keystream[i]);
            offset += chunk;
        }
        return output;
    }

    private static void Block(uint[] input, byte[] outBytes)
    {
        var x = new uint[16];
        Array.Copy(input, x, 16);

        for (int i = 0; i < Rounds; i += 2)
        {
            // column rounds
            QuarterRound(x, 0, 4, 8, 12);
            QuarterRound(x, 1, 5, 9, 13);
            QuarterRound(x, 2, 6, 10, 14);
            QuarterRound(x, 3, 7, 11, 15);
            // diagonal rounds
            QuarterRound(x, 0, 5, 10, 15);
            QuarterRound(x, 1, 6, 11, 12);
            QuarterRound(x, 2, 7, 8, 13);
            QuarterRound(x, 3, 4, 9, 14);
        }

        for (int i = 0; i < 16; i++)
        {
            uint v = x[i] + input[i];
            WriteU32(outBytes, i * 4, v);
        }
    }

    private static void QuarterRound(uint[] x, int a, int b, int c, int d)
    {
        x[a] += x[b]; x[d] = Rotl(x[d] ^ x[a], 16);
        x[c] += x[d]; x[b] = Rotl(x[b] ^ x[c], 12);
        x[a] += x[b]; x[d] = Rotl(x[d] ^ x[a], 8);
        x[c] += x[d]; x[b] = Rotl(x[b] ^ x[c], 7);
    }

    private static uint Rotl(uint v, int n) => (v << n) | (v >> (32 - n));

    private static uint ReadU32(byte[] b, int o) =>
        (uint)(b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24));

    private static void WriteU32(byte[] b, int o, uint v)
    {
        b[o] = (byte)v;
        b[o + 1] = (byte)(v >> 8);
        b[o + 2] = (byte)(v >> 16);
        b[o + 3] = (byte)(v >> 24);
    }
}
