package ori.app;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.graphics.Color;
import android.graphics.Paint;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;

// A GUI Todo app. The list logic is the Ori model (mobile/ori/main.ori) running
// on the native C VM (libori.so) via JNI; this Activity is just the view.
public class MainActivity extends Activity {
    private LinearLayout listContainer;
    private EditText input;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(40, 48, 40, 40);
        root.setBackgroundColor(Color.rgb(15, 17, 22));

        TextView title = new TextView(this);
        title.setText("Ori Todo");
        title.setTextColor(Color.WHITE);
        title.setTextSize(24);

        TextView sub = new TextView(this);
        sub.setText("logic runs on the Ori VM (native C, via JNI)");
        sub.setTextColor(Color.rgb(154, 160, 170));
        sub.setTextSize(12);
        sub.setPadding(0, 2, 0, 18);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        input = new EditText(this);
        input.setHint("What needs doing?");
        input.setTextColor(Color.WHITE);
        input.setHintTextColor(Color.rgb(110, 116, 128));
        input.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        Button add = new Button(this);
        add.setText("Add");
        row.addView(input);
        row.addView(add);

        listContainer = new LinearLayout(this);
        listContainer.setOrientation(LinearLayout.VERTICAL);
        listContainer.setPadding(0, 18, 0, 0);
        ScrollView sc = new ScrollView(this);
        sc.addView(listContainer);

        root.addView(title);
        root.addView(sub);
        root.addView(row);
        root.addView(sc);
        setContentView(root);

        try { OriBridge.runImage(readAsset("app.orb")); }   // boot the model + seed
        catch (Exception e) { }

        add.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { doAdd(); }
        });
        render();
    }

    private void doAdd() {
        String t = input.getText().toString().trim();
        if (t.length() == 0) return;
        OriBridge.call("add", t);
        input.setText("");
        render();
    }

    private void render() {
        listContainer.removeAllViews();
        String view = OriBridge.call("view", "");
        String[] lines = view.split("\n");
        int shown = 0;
        for (String line : lines) {
            if (line.length() == 0) continue;
            String[] p = line.split("\\|");
            if (p.length < 3) continue;
            shown++;
            final String idx = p[0];
            boolean done = p[1].equals("1");
            StringBuilder tb = new StringBuilder();
            for (int k = 2; k < p.length; k++) { if (k > 2) tb.append("|"); tb.append(p[k]); }
            final String text = tb.toString();

            LinearLayout r = new LinearLayout(this);
            r.setOrientation(LinearLayout.HORIZONTAL);
            r.setPadding(0, 16, 0, 16);

            TextView tv = new TextView(this);
            tv.setText((done ? "[x]  " : "[ ]  ") + text);
            tv.setTextColor(done ? Color.rgb(107, 114, 128) : Color.rgb(230, 233, 240));
            tv.setTextSize(17);
            if (done) tv.setPaintFlags(tv.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
            tv.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
            tv.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) { OriBridge.call("toggle", idx); render(); }
            });

            Button del = new Button(this);
            del.setText("X");
            del.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) { OriBridge.call("remove", idx); render(); }
            });

            r.addView(tv);
            r.addView(del);
            listContainer.addView(r);
        }
        if (shown == 0) {
            TextView empty = new TextView(this);
            empty.setText("No tasks yet - add one above.");
            empty.setTextColor(Color.rgb(110, 116, 128));
            listContainer.addView(empty);
        }
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
