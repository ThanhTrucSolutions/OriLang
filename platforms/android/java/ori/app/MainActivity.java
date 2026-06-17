package ori.app;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
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

// GENERIC host: it renders whatever the Ori program describes via render() and
// forwards events to dispatch(event, arg). It contains NO app logic — the todo
// behaviour lives entirely in ori/main.ori. Widget protocol:
//   text|CONTENT
//   edit|PLACEHOLDER
//   btn|EVENT|ARG|CAPTION          (ARG "@edit" -> current text-field contents)
//   item|TAP_EVENT|DEL_EVENT|ARG|CAPTION
public class MainActivity extends Activity {
    private LinearLayout content;
    private String editText = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(40, 44, 40, 40);
        content.setBackgroundColor(Color.rgb(15, 17, 22));
        ScrollView sc = new ScrollView(this);
        sc.setBackgroundColor(Color.rgb(15, 17, 22));
        sc.addView(content);
        setContentView(sc);

        try { OriBridge.runImage(readAsset("app.orb")); }   // boot the Ori program
        catch (Exception e) { }
        build(OriBridge.call("render", ""));
    }

    private void dispatch(String ev, String arg) {
        build(OriBridge.call2("dispatch", ev, arg));
    }

    private void build(String spec) {
        content.removeAllViews();
        String[] lines = spec.split("\n");
        for (String line : lines) {
            if (line.length() == 0) continue;
            int bar = line.indexOf('|');
            String type = bar < 0 ? line : line.substring(0, bar);

            if (type.equals("text")) {
                String[] p = line.split("\\|", 2);
                TextView tv = new TextView(this);
                tv.setText(p.length > 1 ? p[1] : "");
                tv.setTextColor(Color.rgb(230, 233, 240));
                tv.setTextSize(20);
                tv.setPadding(0, 6, 0, 6);
                content.addView(tv);

            } else if (type.equals("edit")) {
                String[] p = line.split("\\|", 2);
                final EditText et = new EditText(this);
                et.setHint(p.length > 1 ? p[1] : "");
                et.setText(editText);
                et.setTextColor(Color.WHITE);
                et.setHintTextColor(Color.rgb(110, 116, 128));
                et.addTextChangedListener(new TextWatcher() {
                    public void afterTextChanged(Editable s) { editText = s.toString(); }
                    public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
                    public void onTextChanged(CharSequence s, int a, int b, int c) {}
                });
                content.addView(et);

            } else if (type.equals("btn")) {
                String[] p = line.split("\\|", 4);
                final String ev = p.length > 1 ? p[1] : "";
                final String argSpec = p.length > 2 ? p[2] : "";
                Button b = new Button(this);
                b.setText(p.length > 3 ? p[3] : "");
                b.setOnClickListener(new View.OnClickListener() {
                    public void onClick(View v) {
                        String a = argSpec;
                        if (argSpec.equals("@edit")) { a = editText; editText = ""; }
                        dispatch(ev, a);
                    }
                });
                content.addView(b);

            } else if (type.equals("item")) {
                String[] p = line.split("\\|", 5);
                final String tap = p.length > 1 ? p[1] : "";
                final String del = p.length > 2 ? p[2] : "";
                final String arg = p.length > 3 ? p[3] : "";
                String cap = p.length > 4 ? p[4] : "";
                LinearLayout row = new LinearLayout(this);
                row.setOrientation(LinearLayout.HORIZONTAL);
                row.setPadding(0, 16, 0, 16);
                TextView tv = new TextView(this);
                tv.setText(cap);
                tv.setTextSize(17);
                boolean done = cap.startsWith("[x]");
                tv.setTextColor(done ? Color.rgb(107, 114, 128) : Color.rgb(230, 233, 240));
                if (done) tv.setPaintFlags(tv.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
                tv.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
                tv.setOnClickListener(new View.OnClickListener() {
                    public void onClick(View v) { dispatch(tap, arg); }
                });
                Button x = new Button(this);
                x.setText("X");
                x.setOnClickListener(new View.OnClickListener() {
                    public void onClick(View v) { dispatch(del, arg); }
                });
                row.addView(tv);
                row.addView(x);
                content.addView(row);
            }
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
