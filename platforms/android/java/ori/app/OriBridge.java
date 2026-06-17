package ori.app;

public class OriBridge {
    static { System.loadLibrary("ori"); }
    public static native String runImage(byte[] orb);          // boot the VM + run main
    public static native String call(String fn, String arg);    // call fn(arg)
    public static native String call2(String fn, String a1, String a2); // call fn(a1, a2)
}
