import string
import tkinter as tk
from tkinter import ttk, messagebox


# =====================================================================
# מודול לוגיקת האניגמה (Backend Engine)
# =====================================================================

class Rotor:
    """מייצג רוטור יחיד במכונת האניגמה כולל חיווט, היסט טבעת ו-Notch."""
    
    WIRINGS = {
        'I':   {'wiring': "EKMFLGDQVZNTOWYHXUSPAIBRCJ", 'notch': 'Q'},
        'II':  {'wiring': "AJDKSIRUXBLHWTMCQGZNPYFVOE", 'notch': 'E'},
        'III': {'wiring': "BDFHJLCPRTXVZNYEIWGAKMUSQO", 'notch': 'V'}
    }

    def __init__(self, model_name: str, position: str = 'A', ring_setting: int = 1):
        self.model_name = model_name
        self.wiring_str = self.WIRINGS[model_name]['wiring']
        self.notch = self.WIRINGS[model_name]['notch']
        
        # המרת חיווט למערכים מספריים לביצועים מהירים
        self.forward_map = [ord(c) - 65 for c in self.wiring_str]
        self.backward_map = [0] * 26
        for i, val in enumerate(self.forward_map):
            self.backward_map[val] = i
            
        self.position = ord(position.upper()) - 65
        self.ring = ring_setting - 1  # המרה מ-1..26 ל-0..25

    def is_at_notch(self) -> bool:
        """בדיקה האם הרוטור נמצא בעמדה שמפעילה את ה-Notch שלו."""
        notch_idx = ord(self.notch) - 65
        return self.position == notch_idx

    def step(self):
        """קידום הרוטור בצעד אחד."""
        self.position = (self.position + 1) % 26

    def forward(self, char_code: int) -> int:
        """מעבר אות בכיוון ימין-לשמאל (לכיוון המשקף)."""
        offset = self.position - self.ring
        entry = (char_code + offset) % 26
        out = self.forward_map[entry]
        return (out - offset) % 26

    def backward(self, char_code: int) -> int:
        """מעבר אות בכיוון שמאל-לימין (חזרה מהמשקף)."""
        offset = self.position - self.ring
        entry = (char_code + offset) % 26
        out = self.backward_map[entry]
        return (out - offset) % 26


class Reflector:
    """משקף תקני (Reflector B הצבאי)."""
    
    WIRING_B = "YRUHQSLDPXNGOKMIEBFZCWVJAT"

    def __init__(self):
        self.mapping = [ord(c) - 65 for c in self.WIRING_B]

    def reflect(self, char_code: int) -> int:
        return self.mapping[char_code]


class Plugboard:
    """לוח תקעים (Steckerbrett) המבצע החלפה הדדית בין זוגות אותיות."""
    
    def __init__(self, pairs_str: str = ""):
        self.mapping = list(range(26))
        self.set_pairs(pairs_str)

    def set_pairs(self, pairs_str: str):
        self.mapping = list(range(26))
        cleaned = pairs_str.upper().strip().split()
        used = set()
        
        for pair in cleaned:
            if len(pair) != 2 or not pair.isalpha():
                raise ValueError(f"זוג תקעים לא חוקי: '{pair}'. חובה זוג אותיות בדיוק (למשל AB).")
            c1, c2 = pair[0], pair[1]
            if c1 == c2:
                raise ValueError(f"אות אינה יכולה לחבור לעצמה: '{pair}'.")
            if c1 in used or c2 in used:
                raise ValueError(f"אות מוגדרת יותר מפעם אחת בלוח התקעים: '{pair}'.")
            
            used.add(c1)
            used.add(c2)
            idx1, idx2 = ord(c1) - 65, ord(c2) - 65
            self.mapping[idx1] = idx2
            self.mapping[idx2] = idx1

    def swap(self, char_code: int) -> int:
        return self.mapping[char_code]


class EnigmaMachine:
    """הרכבה מלאה של מכונת האניגמה: לוח תקעים, 3 רוטורים ומשקף."""
    
    def __init__(self, rotor_names=('I', 'II', 'III'), 
                 positions=('A', 'A', 'A'), 
                 rings=(1, 1, 1), 
                 plugboard_str=""):
        self.rotor_left = Rotor(rotor_names[0], positions[0], rings[0])
        self.rotor_middle = Rotor(rotor_names[1], positions[1], rings[1])
        self.rotor_right = Rotor(rotor_names[2], positions[2], rings[2])
        self.reflector = Reflector()
        self.plugboard = Plugboard(plugboard_str)

    def set_positions(self, pos_left: str, pos_mid: str, pos_right: str):
        self.rotor_left.position = ord(pos_left.upper()) - 65
        self.rotor_middle.position = ord(pos_mid.upper()) - 65
        self.rotor_right.position = ord(pos_right.upper()) - 65

    def get_positions(self):
        return (
            chr(self.rotor_left.position + 65),
            chr(self.rotor_middle.position + 65),
            chr(self.rotor_right.position + 65)
        )

    def step_rotors(self):
        """
        מימוש מנגנון הקידום ההיסטורי כולל Double-Stepping.
        הקידום מתרחש לפני מעבר הזרם החשמלי באות.
        """
        # בדיקת נקודות Notch
        mid_at_notch = self.rotor_middle.is_at_notch()
        right_at_notch = self.rotor_right.is_at_notch()

        # אם הרוטור האמצעי ב-Notch, הוא מקדם את השמאלי וגם את עצמו (צעד כפול)
        if mid_at_notch:
            self.rotor_middle.step()
            self.rotor_left.step()
        # אם הרוטור הימני ב-Notch (והאמצעי טרם קודם מהתנאי הקודם), האמצעי מקודם
        elif right_at_notch:
            self.rotor_middle.step()

        # הרוטור הימני מקודם תמיד בכל לחיצה
        self.rotor_right.step()

    def process_char(self, char: str) -> str:
        """הצפנה/פענוח של תו בודד (A-Z)."""
        if not char.isalpha():
            return char
            
        c = ord(char.upper()) - 65
        
        # 1. מנגנון תנועת הרוטורים
        self.step_rotors()

        # 2. כניסה ללוח התקעים
        c = self.plugboard.swap(c)

        # 3. מעבר ברוטורים (מימין לשמאל)
        c = self.rotor_right.forward(c)
        c = self.rotor_middle.forward(c)
        c = self.rotor_left.forward(c)

        # 4. מעבר במשקף
        c = self.reflector.reflect(c)

        # 5. חזרה דרך הרוטורים (משמאל לימין)
        c = self.rotor_left.backward(c)
        c = self.rotor_middle.backward(c)
        c = self.rotor_right.backward(c)

        # 6. יציאה מלוח התקעים
        c = self.plugboard.swap(c)

        return chr(c + 65)


# =====================================================================
# ממשק משתמש גרפי (Tkinter GUI)
# =====================================================================

class EnigmaGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Wehrmacht Enigma M3 Simulator")
        self.root.geometry("860x780")
        self.root.configure(bg="#222222")

        # שמירת עמדות התחלה להבטחת הדדיות מלאה בטקסט מלא
        self.start_positions = ['A', 'A', 'A']
        self.enigma = None

        self._build_styles()
        self._build_ui()
        self.rebuild_enigma()
        
        # חיבור מקלדת פיזית
        self.root.bind("<Key>", self.handle_key_press)

    def _build_styles(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure("TLabel", background="#222222", foreground="#E0E0E0", font=("Segoe UI", 10))
        style.configure("Header.TLabel", font=("Segoe UI", 12, "bold"), foreground="#FFCC00")
        style.configure("TCombobox", fieldbackground="#333333", background="#444444", foreground="#FFFFFF")

    def _build_ui(self):
        # 1. פנל שליטה ברוטורים והגדרות
        top_frame = tk.LabelFrame(self.root, text=" Rotor Settings & Plugboard ", 
                                  bg="#2A2A2A", fg="#FFCC00", font=("Segoe UI", 11, "bold"), padx=15, pady=10)
        top_frame.pack(fill=tk.X, padx=15, pady=10)

        # כותרות עמודות
        headers = ["Position", "Model", "Ring Setting", "Current Letter", "Adjust"]
        for col_idx, h in enumerate(headers):
            lbl = tk.Label(top_frame, text=h, bg="#2A2A2A", fg="#AAAAAA", font=("Segoe UI", 9, "bold"))
            lbl.grid(row=0, column=col_idx, padx=8, pady=4)

        self.rotor_models = []
        self.ring_settings = []
        self.pos_labels = []
        self.rotor_positions = ['A', 'A', 'A']

        labels = ["Left (L)", "Middle (M)", "Right (R)"]
        default_models = ["I", "II", "III"]

        for i in range(3):
            # מיקום
            tk.Label(top_frame, text=labels[i], bg="#2A2A2A", fg="#FFFFFF", font=("Segoe UI", 10)).grid(row=i+1, column=0, padx=8, pady=4)
            
            # בחירת דגם
            model_cb = ttk.Combobox(top_frame, values=["I", "II", "III"], width=6, state="readonly")
            model_cb.set(default_models[i])
            model_cb.grid(row=i+1, column=1, padx=8, pady=4)
            model_cb.bind("<<ComboboxSelected>>", lambda e: self.rebuild_enigma())
            self.rotor_models.append(model_cb)

            # Ring Setting
            ring_cb = ttk.Combobox(top_frame, values=[str(n) for n in range(1, 27)], width=6, state="readonly")
            ring_cb.set("1")
            ring_cb.grid(row=i+1, column=2, padx=8, pady=4)
            ring_cb.bind("<<ComboboxSelected>>", lambda e: self.rebuild_enigma())
            self.ring_settings.append(ring_cb)

            # חלון תצוגת אות
            pos_lbl = tk.Label(top_frame, text=self.rotor_positions[i], bg="#111111", fg="#00FF66",
                               font=("Consolas", 14, "bold"), width=4, relief="sunken", bd=2)
            pos_lbl.grid(row=i+1, column=3, padx=8, pady=4)
            self.pos_labels.append(pos_lbl)

            # כפתורי ▲/▼
            btn_frame = tk.Frame(top_frame, bg="#2A2A2A")
            btn_frame.grid(row=i+1, column=4, padx=8, pady=4)
            tk.Button(btn_frame, text="▲", width=2, bg="#444444", fg="#FFFFFF", relief="groove",
                      command=lambda idx=i: self.adjust_rotor(idx, 1)).pack(side=tk.LEFT, padx=1)
            tk.Button(btn_frame, text="▼", width=2, bg="#444444", fg="#FFFFFF", relief="groove",
                      command=lambda idx=i: self.adjust_rotor(idx, -1)).pack(side=tk.LEFT, padx=1)

        # לוח תקעים (Plugboard)
        pb_frame = tk.Frame(top_frame, bg="#2A2A2A")
        pb_frame.grid(row=4, column=0, columnspan=5, pady=10, sticky="ew")

        tk.Label(pb_frame, text="Plugboard Pairs (e.g., AB CD EF):", bg="#2A2A2A", fg="#FFFFFF", font=("Segoe UI", 10)).pack(side=tk.LEFT, padx=5)
        self.plugboard_entry = tk.Entry(pb_frame, bg="#333333", fg="#FFFFFF", insertbackground="white", width=30, font=("Consolas", 11))
        self.plugboard_entry.pack(side=tk.LEFT, padx=5)
        
        tk.Button(pb_frame, text="Apply", bg="#006699", fg="#FFFFFF", relief="groove", padx=8,
                  command=self.rebuild_enigma).pack(side=tk.LEFT, padx=5)
        tk.Button(pb_frame, text="Clear", bg="#555555", fg="#FFFFFF", relief="groove", padx=8,
                  command=self.clear_plugboard).pack(side=tk.LEFT, padx=2)

        # 2. לוח נורות ומקלדת (Lampboard & Keyboard)
        board_frame = tk.LabelFrame(self.root, text=" Lampboard & Keyboard ", bg="#222222", 
                                    fg="#FFCC00", font=("Segoe UI", 11, "bold"), padx=10, pady=8)
        board_frame.pack(fill=tk.X, padx=15, pady=5)

        layout = ["QWERTZUIO", "ASDFGHJK", "PYXCVBNML"]
        
        # Lampboard (נורות)
        lamp_container = tk.Frame(board_frame, bg="#222222")
        lamp_container.pack(pady=4)
        self.lamps = {}

        for row_str in layout:
            r_frame = tk.Frame(lamp_container, bg="#222222")
            r_frame.pack(pady=2)
            for ch in row_str:
                lbl = tk.Label(r_frame, text=ch, width=3, height=1, bg="#333333", fg="#777777",
                               font=("Consolas", 11, "bold"), relief="raised", bd=2)
                lbl.pack(side=tk.LEFT, padx=3)
                self.lamps[ch] = lbl

        # Keyboard (מקלדת פיזית על המסך)
        key_container = tk.Frame(board_frame, bg="#222222")
        key_container.pack(pady=4)
        for row_str in layout:
            r_frame = tk.Frame(key_container, bg="#222222")
            r_frame.pack(pady=2)
            for ch in row_str:
                btn = tk.Button(r_frame, text=ch, width=3, bg="#111111", fg="#E0E0E0", relief="raised", bd=2,
                                font=("Consolas", 10, "bold"),
                                command=lambda c=ch: self.type_character(c))
                btn.pack(side=tk.LEFT, padx=3)

        # 3. טיפול בטקסט מלא (Full Text Processing)
        text_frame = tk.LabelFrame(self.root, text=" Batch Text Processing ", bg="#2A2A2A", 
                                   fg="#FFCC00", font=("Segoe UI", 11, "bold"), padx=10, pady=8)
        text_frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=8)

        # בקרות עיצוב ופעולה
        ctrl_frame = tk.Frame(text_frame, bg="#2A2A2A")
        ctrl_frame.pack(fill=tk.X, pady=4)

        self.format_mode = tk.StringVar(value="KEEP")
        tk.Radiobutton(ctrl_frame, text="Keep Punctuation & Spaces", variable=self.format_mode, 
                       value="KEEP", bg="#2A2A2A", fg="#FFFFFF", selectcolor="#444444").pack(side=tk.LEFT, padx=10)
        tk.Radiobutton(ctrl_frame, text="Military Format (5-Letter Blocks)", variable=self.format_mode, 
                       value="MILITARY", bg="#2A2A2A", fg="#FFFFFF", selectcolor="#444444").pack(side=tk.LEFT, padx=10)

        tk.Button(ctrl_frame, text="Encrypt / Decrypt Full Text", bg="#008844", fg="#FFFFFF", 
                  font=("Segoe UI", 9, "bold"), relief="groove", padx=10,
                  command=self.process_full_text).pack(side=tk.RIGHT, padx=5)
        tk.Button(ctrl_frame, text="⇄ Swap Input/Output", bg="#555577", fg="#FFFFFF", 
                  relief="groove", padx=8, command=self.swap_io).pack(side=tk.RIGHT, padx=5)

        # תיבות קלט ופלט
        io_container = tk.Frame(text_frame, bg="#2A2A2A")
        io_container.pack(fill=tk.BOTH, expand=True, pady=4)

        input_box_frame = tk.Frame(io_container, bg="#2A2A2A")
        input_box_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=4)
        tk.Label(input_box_frame, text="Input Text:", bg="#2A2A2A", fg="#FFFFFF").pack(anchor="w")
        self.input_text = tk.Text(input_box_frame, height=5, bg="#1E1E1E", fg="#FFFFFF", 
                                  insertbackground="white", font=("Consolas", 10))
        self.input_text.pack(fill=tk.BOTH, expand=True)

        output_box_frame = tk.Frame(io_container, bg="#2A2A2A")
        output_box_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=4)
        tk.Label(output_box_frame, text="Output Text:", bg="#2A2A2A", fg="#FFFFFF").pack(anchor="w")
        self.output_text = tk.Text(output_box_frame, height=5, bg="#1E1E1E", fg="#00FF66", 
                                   insertbackground="white", font=("Consolas", 10))
        self.output_text.pack(fill=tk.BOTH, expand=True)

    # =====================================================================
    # לוגיקת סנכרון וניהול מצב
    # =====================================================================

    def rebuild_enigma(self):
        """בנייה מחדש של מכונת האניגמה בהתאם להגדרות הנוכחיות בממשק."""
        try:
            models = [cb.get() for cb in self.rotor_models]
            rings = [int(cb.get()) for cb in self.ring_settings]
            pb_str = self.plugboard_entry.get()
            
            self.enigma = EnigmaMachine(
                rotor_names=models,
                positions=self.rotor_positions,
                rings=rings,
                plugboard_str=pb_str
            )
            # שמירת מיקום הפתיחה
            self.start_positions = list(self.rotor_positions)
        except ValueError as ex:
            messagebox.showerror("Invalid Configuration", str(ex))

    def clear_plugboard(self):
        self.plugboard_entry.delete(0, tk.END)
        self.rebuild_enigma()

    def update_position_display(self):
        """עדכון תצוגת האותיות של הרוטורים בחלונות."""
        for i in range(3):
            self.pos_labels[i].config(text=self.rotor_positions[i])

    def adjust_rotor(self, rotor_idx: int, delta: int):
        """שינוי ידני של מיקום רוטור באמצעות כפתורי החצים."""
        curr_val = ord(self.rotor_positions[rotor_idx]) - 65
        new_val = (curr_val + delta) % 26
        self.rotor_positions[rotor_idx] = chr(new_val + 65)
        self.start_positions[rotor_idx] = self.rotor_positions[rotor_idx]
        self.update_position_display()
        if self.enigma:
            self.enigma.set_positions(*self.rotor_positions)

    def illuminate_lamp(self, char: str):
        """הדלקת נורה במנורת האניגמה למשך 450 מילי-שניות."""
        if char in self.lamps:
            lbl = self.lamps[char]
            lbl.config(bg="#FFEA00", fg="#000000")
            self.root.after(450, lambda: lbl.config(bg="#333333", fg="#777777"))

    def handle_key_press(self, event):
        """זיהוי הקלדה במקלדת הפיזית כשהפוקוס אינו בתיבות הטקסט."""
        if self.root.focus_get() in (self.input_text, self.output_text, self.plugboard_entry):
            return
        ch = event.char.upper()
        if ch in string.ascii_uppercase:
            self.type_character(ch)

    def type_character(self, char: str):
        """הקלדת תו יחיד: קידום הרוטורים, הצפנה, הדלקת נורה ופליטה לממשק."""
        if not self.enigma:
            self.rebuild_enigma()
            
        out_char = self.enigma.process_char(char)
        self.rotor_positions = list(self.enigma.get_positions())
        self.update_position_display()
        self.illuminate_lamp(out_char)
        
        # הוספה לתיבת הפלט
        self.output_text.insert(tk.END, out_char)
        self.output_text.see(tk.END)

    def process_full_text(self):
        """
        הצפנה/פענוח של כל הטקסט שבתיבת הקלט.
        קריטי: מתחיל תמיד מעמדת הפתיחה שהוגדרה מראש (start_positions),
        כדי להבטיח שפענוח חוזר יחזיר במדויק את המקור.
        """
        raw_text = self.input_text.get("1.0", tk.END).strip()
        if not raw_text:
            return

        # אתחול המכונה בדיוק למצב הפתיחה שהוגדר
        self.rotor_positions = list(self.start_positions)
        self.rebuild_enigma()
        self.update_position_display()

        mode = self.format_mode.get()
        result_chars = []

        if mode == "KEEP":
            for ch in raw_text:
                if ch.upper() in string.ascii_uppercase:
                    is_lower = ch.islower()
                    enc = self.enigma.process_char(ch)
                    result_chars.append(enc.lower() if is_lower else enc)
                else:
                    result_chars.append(ch)
            final_text = "".join(result_chars)

        else:  # MILITARY: אותיות בלבד בחלוקה לקבוצות של 5
            only_letters = [ch.upper() for ch in raw_text if ch.upper() in string.ascii_uppercase]
            for ch in only_letters:
                result_chars.append(self.enigma.process_char(ch))
            
            # קיבוץ לבלוקים של 5
            groups = []
            for i in range(0, len(result_chars), 5):
                groups.append("".join(result_chars[i:i+5]))
            final_text = " ".join(groups)

        # עדכון תצוגת הפלט והרוטורים בסיום הריצה
        self.output_text.delete("1.0", tk.END)
        self.output_text.insert(tk.END, final_text)
        self.rotor_positions = list(self.enigma.get_positions())
        self.update_position_display()

    def swap_io(self):
        """החלפה בין תיבת הקלט לתיבת הפלט לבדיקת הדדיות מהירה."""
        out_content = self.output_text.get("1.0", tk.END).strip()
        self.input_text.delete("1.0", tk.END)
        self.input_text.insert(tk.END, out_content)
        self.output_text.delete("1.0", tk.END)


if __name__ == "__main__":
    app_root = tk.Tk()
    app = EnigmaGUI(app_root)
    app_root.mainloop()