package ori.app;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.graphics.Color;
import android.graphics.Typeface;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;

public class MainActivity extends Activity {
    private TextView out;
    private byte[] orb;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(40, 48, 40, 40);
        root.setBackgroundColor(Color.rgb(24, 26, 32));

        TextView title = new TextView(this);
        title.setText("Ori VM on Android");
        title.setTextColor(Color.WHITE);
        title.setTextSize(22);
        title.setPadding(0, 0, 0, 6);

        TextView sub = new TextView(this);
        sub.setText("the native C VM, cross-compiled for the device");
        sub.setTextColor(Color.rgb(154, 160, 170));
        sub.setTextSize(13);
        sub.setPadding(0, 0, 0, 20);

        Button btn = new Button(this);
        btn.setText("Run Ori VM");

        out = new TextView(this);
        out.setTextColor(Color.rgb(120, 230, 180));
        out.setTypeface(Typeface.MONOSPACE);
        out.setTextSize(13);
        out.setPadding(0, 24, 0, 0);

        ScrollView sc = new ScrollView(this);
        sc.addView(out);

        root.addView(title);
        root.addView(sub);
        root.addView(btn);
        root.addView(sc);
        setContentView(root);

        try { orb = readAsset("app.orb"); }
        catch (Exception e) { out.setText("load error: " + e); }

        btn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { runVm(); }
        });
        runVm();
    }

    private void runVm() {
        try { out.setText(OriBridge.runImage(orb)); }
        catch (Throwable t) { out.setText("error: " + t); }
    }

    private byte[] readAsset(String name) throws Exception {
        InputStream is = getAssets().open(name);
        ByteArrayOutputStream bo = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int r;
        while ((r = is.read(buf)) > 0) bo.write(buf, 0, r);
        is.close();
        return bo.toByteArray();
    }
}
