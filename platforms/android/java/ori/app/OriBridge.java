package ori.app;

public class OriBridge {
    static { System.loadLibrary("ori"); }
    public static native String runImage(byte[] orb);
    public static native String call(String fn, String arg);
}
