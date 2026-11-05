package com.limelight.clouddeck;

import android.app.Activity;
import android.os.Bundle;
import android.text.InputType;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * CloudDeck-Client: edit controller remapping profiles on Android
 * (docs/controller-profiles.md). Same sections as the desktop editor: profile
 * picker, buttons, sticks and triggers, which controllers use which profile.
 */
public class ControllerProfilesActivity extends Activity {
    private static final String[] BUTTON_LABELS = {
            "A (bottom)", "B (right)", "X (left)", "Y (top)", "Back / Select / Share", "Guide / Home / PS",
            "Start / Menu / Options", "Left stick click", "Right stick click", "Left bumper (LB / L1)",
            "Right bumper (RB / R1)", "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Capture / Mic",
            "Back paddle 1", "Back paddle 2", "Back paddle 3", "Back paddle 4", "Touchpad click"};

    private LinearLayout root;
    private ControllerProfile editing;   // working copy
    private String originalName;         // null = new profile

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ScrollView scroll = new ScrollView(this);
        root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int p = dp(16);
        root.setPadding(p, p, p, p);
        scroll.addView(root);
        setContentView(scroll);

        List<ControllerProfile> list = ControllerProfile.Store.profiles(this);
        if (list.isEmpty()) startNew();
        else edit(list.get(0));
    }

    private void startNew() {
        editing = new ControllerProfile();
        editing.name = newName();
        originalName = null;
        render();
    }

    private void edit(ControllerProfile p) {
        editing = ControllerProfile.fromJson(p.toJson());
        originalName = p.name;
        render();
    }

    private String newName() {
        Set<String> names = new LinkedHashSet<>();
        for (ControllerProfile p : ControllerProfile.Store.profiles(this)) names.add(p.name);
        for (int i = 1; ; i++) {
            String n = i == 1 ? "My profile" : "My profile " + i;
            if (!names.contains(n)) return n;
        }
    }

    private void render() {
        root.removeAllViews();
        List<ControllerProfile> list = ControllerProfile.Store.profiles(this);
        List<String> names = new ArrayList<>();
        for (ControllerProfile x : list) names.add(x.name);

        // ---- which profile
        header("Profile");
        if (!names.isEmpty()) {
            Spinner picker = spinner(names, Math.max(0, names.indexOf(originalName)));
            picker.setOnItemSelectedListener(onSelect(i -> {
                if (!names.get(i).equals(originalName)) edit(list.get(i));
            }));
            root.addView(picker);
        }
        LinearLayout row = new LinearLayout(this);
        row.addView(button("New", v -> startNew()));
        Button del = button("Delete", v -> {
            List<ControllerProfile> l = ControllerProfile.Store.profiles(this);
            l.removeIf(x -> x.name.equals(originalName));
            ControllerProfile.Store.saveProfiles(this, l);
            if (originalName != null && originalName.equals(ControllerProfile.Store.defaultProfile(this))) {
                ControllerProfile.Store.setDefaultProfile(this, "");
            }
            if (l.isEmpty()) startNew();
            else edit(l.get(0));
        });
        del.setEnabled(originalName != null);
        row.addView(del);
        root.addView(row);
        EditText name = new EditText(this);
        name.setInputType(InputType.TYPE_CLASS_TEXT);
        name.setText(editing.name);
        name.setOnFocusChangeListener((v, focus) -> { if (!focus) editing.name = name.getText().toString().trim(); });
        root.addView(name);

        // ---- buttons
        header("Buttons");
        note("Each button on the left sends the one on the right. The Xbox/Nintendo switch in the stream menu applies before this.");
        List<String> targetLabels = new ArrayList<>();
        for (String l : BUTTON_LABELS) targetLabels.add(l);
        targetLabels.add("(disabled)");
        for (int i = 0; i < ControllerProfile.BUTTONS.size(); i++) {
            final String src = ControllerProfile.BUTTONS.get(i);
            LinearLayout r = new LinearLayout(this);
            r.setOrientation(LinearLayout.HORIZONTAL);
            TextView t = new TextView(this);
            t.setText(BUTTON_LABELS[i]);
            r.addView(t, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            String target = editing.mapButton(src);
            int sel = target.equals("none") ? ControllerProfile.BUTTONS.size() : ControllerProfile.BUTTONS.indexOf(target);
            Spinner s = spinner(targetLabels, sel);
            s.setOnItemSelectedListener(onSelect(k -> {
                String tgt = k == ControllerProfile.BUTTONS.size() ? "none" : ControllerProfile.BUTTONS.get(k);
                if (tgt.equals(src)) editing.buttons.remove(src);
                else editing.buttons.put(src, tgt);
            }));
            r.addView(s, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            root.addView(r);
        }

        // ---- sticks and triggers
        header("Sticks and triggers");
        stick("Left stick", editing.leftStick);
        stick("Right stick", editing.rightStick);
        slider("Trigger deadzone", 0, 50, (int) Math.round(editing.triggerDeadzone * 100), "%",
                v -> editing.triggerDeadzone = v / 100.0);
        root.addView(check("Swap left and right stick", editing.swapSticks, c -> editing.swapSticks = c));

        root.addView(button("Save profile", v -> save()));

        // ---- assignment
        header("Which controllers use which profile");
        List<String> withNone = new ArrayList<>();
        withNone.add("No remapping");
        withNone.addAll(names);
        LinearLayout dr = new LinearLayout(this);
        TextView dl = new TextView(this);
        dl.setText("All controllers (default)");
        dr.addView(dl, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        Spinner def = spinner(withNone, names.indexOf(ControllerProfile.Store.defaultProfile(this)) + 1);
        def.setOnItemSelectedListener(onSelect(i -> ControllerProfile.Store.setDefaultProfile(this, i == 0 ? "" : names.get(i - 1))));
        dr.addView(def, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        root.addView(dr);

        Map<String, String> assigned = ControllerProfile.Store.assignments(this);
        List<String> useDefault = new ArrayList<>();
        useDefault.add("Use the default");
        useDefault.addAll(names);
        List<String> controllers = connectedControllers();
        for (String c : controllers) {
            LinearLayout cr = new LinearLayout(this);
            TextView cl = new TextView(this);
            cl.setText(c);
            cr.addView(cl, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            String a = assigned.get(c);
            Spinner cs = spinner(useDefault, a == null ? 0 : names.indexOf(a) + 1);
            cs.setOnItemSelectedListener(onSelect(i -> ControllerProfile.Store.setAssignment(this, c, i == 0 ? "" : names.get(i - 1))));
            cr.addView(cs, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            root.addView(cr);
        }
        if (controllers.isEmpty()) note("Connect a controller to give it its own profile. The built-in controls of a handheld show up here too.");
        note("Profiles apply from the next stream.");
    }

    private void save() {
        View f = getCurrentFocus();
        if (f != null) f.clearFocus(); // commit the name field
        String n = editing.name == null ? "" : editing.name.trim();
        if (n.isEmpty()) { toast("Give the profile a name."); return; }
        List<ControllerProfile> list = ControllerProfile.Store.profiles(this);
        for (ControllerProfile x : list) {
            if (x.name.equals(n) && !n.equals(originalName)) { toast("A profile named \"" + n + "\" already exists."); return; }
        }
        editing.name = n;
        ControllerProfile clean = ControllerProfile.fromJson(editing.toJson()); // clamps like a loaded profile
        boolean replaced = false;
        for (int i = 0; i < list.size(); i++) {
            if (list.get(i).name.equals(originalName)) { list.set(i, clean); replaced = true; break; }
        }
        if (!replaced) list.add(clean);
        ControllerProfile.Store.saveProfiles(this, list);
        if (originalName != null && !originalName.equals(n)) { // keep references after a rename
            if (originalName.equals(ControllerProfile.Store.defaultProfile(this))) ControllerProfile.Store.setDefaultProfile(this, n);
            for (Map.Entry<String, String> e : ControllerProfile.Store.assignments(this).entrySet()) {
                if (e.getValue().equals(originalName)) ControllerProfile.Store.setAssignment(this, e.getKey(), n);
            }
        }
        originalName = n;
        toast("Saved");
        render();
    }

    private List<String> connectedControllers() {
        Set<String> out = new LinkedHashSet<>();
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null || d.isVirtual()) continue;
            int s = d.getSources();
            if ((s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                    (s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
                out.add(d.getName());
            }
        }
        return new ArrayList<>(out);
    }

    // ---- small view helpers

    private interface IntCb { void on(int v); }
    private interface BoolCb { void on(boolean v); }

    private void stick(String label, ControllerProfile.Stick s) {
        TextView t = new TextView(this);
        t.setText(label);
        t.setPadding(0, dp(8), 0, 0);
        root.addView(t);
        slider("Deadzone", 0, 50, (int) Math.round(s.deadzone * 100), "%", v -> s.deadzone = v / 100.0);
        slider("Sensitivity", 50, 200, (int) Math.round(s.sensitivity * 100), "%", v -> s.sensitivity = v / 100.0);
        root.addView(check("Invert up/down", s.invertY, c -> s.invertY = c));
    }

    private void slider(String label, int min, int max, int value, String unit, IntCb cb) {
        TextView t = new TextView(this);
        t.setText(label + " " + value + unit);
        SeekBar b = new SeekBar(this);
        b.setMax(max - min);
        b.setProgress(value - min);
        b.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                t.setText(label + " " + (progress + min) + unit);
                cb.on(progress + min);
            }
            @Override public void onStartTrackingTouch(SeekBar sb) {}
            @Override public void onStopTrackingTouch(SeekBar sb) {}
        });
        root.addView(t);
        root.addView(b);
    }

    private CheckBox check(String text, boolean checked, BoolCb cb) {
        CheckBox c = new CheckBox(this);
        c.setText(text);
        c.setChecked(checked);
        c.setOnCheckedChangeListener((v, on) -> cb.on(on));
        return c;
    }

    private Button button(String text, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setOnClickListener(l);
        return b;
    }

    private Spinner spinner(List<String> items, int selected) {
        Spinner s = new Spinner(this);
        ArrayAdapter<String> a = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, items);
        a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        s.setAdapter(a);
        if (selected >= 0 && selected < items.size()) s.setSelection(selected, false);
        return s;
    }

    private AdapterView.OnItemSelectedListener onSelect(IntCb cb) {
        return new AdapterView.OnItemSelectedListener() {
            private boolean first = true; // Spinner reports its initial selection; ignore that
            @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (first) { first = false; return; }
                cb.on(position);
            }
            @Override public void onNothingSelected(AdapterView<?> parent) {}
        };
    }

    private void header(String text) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(17);
        t.setTypeface(t.getTypeface(), android.graphics.Typeface.BOLD);
        t.setPadding(0, dp(14), 0, dp(4));
        root.addView(t);
    }

    private void note(String text) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setAlpha(0.7f);
        root.addView(t);
    }

    private void toast(String s) { Toast.makeText(this, s, Toast.LENGTH_SHORT).show(); }

    private int dp(int v) { return Math.round(v * getResources().getDisplayMetrics().density); }
}
