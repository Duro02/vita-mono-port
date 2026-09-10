using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;

class Battery {
    static int passed = 0;
    static int failed = 0;
    static StreamWriter log;

    static void Check(bool cond, string name) {
        if (cond) { passed++; log.WriteLine("PASS " + name); }
        else { failed++; log.WriteLine("FAIL " + name); }
    }

    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/battery-result.txt", false);
        log.AutoFlush = true;
        try {
            T01_Exceptions();
            T02_Threads();
            T03_GcPressure();
            T04_Generics();
            T05_Delegates();
            T06_Reflection();
            T07_ValueTypes();
            T08_FileIO();
            T09_Arrays();
            T10_Math();
            T11_Encoding();
            T12_Collections();
            T13_Strings();
            T14_DateTime();
            T15_Boxing();
        } catch (Exception e) {
            log.WriteLine("FATAL " + e.GetType().Name + ": " + e.Message);
            failed++;
        }
        log.WriteLine("TOTAL passed=" + passed + " failed=" + failed);
        log.Close();
        Console.WriteLine("Battery: passed=" + passed + " failed=" + failed);
        return failed == 0 ? 0 : 1;
    }

    static void T01_Exceptions() {
        try {
            try { throw new InvalidOperationException("x"); }
            catch (InvalidOperationException) { Check(true, "ex-catch"); }
            try { try { throw new Exception("inner"); } finally { Check(true, "ex-finally"); } }
            catch { Check(true, "ex-propagate"); }
            int r = 0;
            try { r = Div(10, 0); } catch (DivideByZeroException) { r = -1; }
            Check(r == -1, "ex-divzero");
        } catch (Exception) { Check(false, "ex-outer"); }
    }
    static int Div(int a, int b) { return a / b; }

    static volatile int tcounter;
    static void T02_Threads() {
        tcounter = 0;
        object lk = new object();
        Thread[] ts = new Thread[4];
        for (int i = 0; i < 4; i++) {
            ts[i] = new Thread(delegate() {
                for (int j = 0; j < 250; j++) {
                    lock (lk) { tcounter++; }
                    Interlocked.Increment(ref tcounter);
                }
            });
            ts[i].Start();
        }
        for (int i = 0; i < 4; i++) ts[i].Join();
        Check(tcounter == 4 * 250 * 2, "thread-lock-interlocked");
        bool done = false;
        ThreadPool.QueueUserWorkItem(delegate(object o) { done = true; }, null);
        for (int i = 0; i < 200 && !done; i++) Thread.Sleep(10);
        Check(done, "threadpool");
    }

    static void T03_GcPressure() {
        ArrayList keep = new ArrayList();
        for (int i = 0; i < 20000; i++) keep.Add(new byte[64]);
        Check(keep.Count == 20000, "gc-many-small");
        keep.Clear();
        byte[] big = new byte[1024 * 1024];
        big[1000] = 7;
        Check(big[1000] == 7, "gc-large");
        big = null;
        object o = new object();
        WeakReference w = new WeakReference(o);
        o = null;
        GC.Collect();
        GC.WaitForPendingFinalizers();
        Check(true, "gc-collect-survived");
        Check(GC.MaxGeneration >= 1, "gc-generations");
    }

    static void T04_Generics() {
        List<int> li = new List<int>();
        for (int i = 0; i < 100; i++) li.Add(i * 2);
        Check(li[50] == 100, "list-int");
        Dictionary<string, int> d = new Dictionary<string, int>();
        d["a"] = 1; d["b"] = 2;
        Check(d["b"] == 2 && d.Count == 2, "dict-str-int");
        int? n = null;
        Check(!n.HasValue, "nullable-null");
        n = 5;
        Check(n.Value == 5, "nullable-val");
        Check(Max(3, 7) == 7 && Max("x", "y") == "y", "generic-method");
        Check(typeof(List<>).IsGenericTypeDefinition, "generic-typedef");
    }
    static T Max<T>(T a, T b) where T : IComparable<T> { return a.CompareTo(b) > 0 ? a : b; }

    static int evCount;
    static void T05_Delegates() {
        Func<int, int> f = delegate(int x) { return x * 3; };
        Check(f(7) == 21, "anon-delegate");
        int cap = 10;
        Func<int> c = delegate() { return cap + 1; };
        Check(c() == 11, "closure");
        Func<int, int> sq = x => x * x;
        Check(sq(9) == 81, "lambda");
        evCount = 0;
        Ev += delegate(object s, EventArgs e) { evCount++; };
        Ev += delegate(object s, EventArgs e) { evCount += 10; };
        FireEv();
        Check(evCount == 11, "event");
    }
    static event EventHandler Ev;
    static void FireEv() { if (Ev != null) Ev(null, EventArgs.Empty); }

    static void T06_Reflection() {
        Type t = typeof(string);
        Check(t.Name == "String", "refl-name");
        MethodInfo mi = t.GetMethod("Concat", new Type[] { typeof(string), typeof(string) });
        object r = mi.Invoke(null, new object[] { "a", "b" });
        Check((string)r == "ab", "refl-invoke-static");
        object sb = Activator.CreateInstance(typeof(StringBuilder));
        Check(sb != null, "activator");
        object[] attrs = typeof(Battery).GetCustomAttributes(false);
        Check(attrs != null, "attrs");
        Check(typeof(int).IsValueType && !typeof(string).IsValueType, "isvaluetype");
    }

    enum Color { Red = 1, Green = 2, Blue = 4 }
    struct Pt { public int X, Y; public Pt(int x, int y) { X = x; Y = y; } }
    static void T07_ValueTypes() {
        Color c = Color.Green;
        Check((int)c == 2 && c.ToString() == "Green", "enum");
        Check(Enum.IsDefined(typeof(Color), 4), "enum-isdefined");
        Pt p = new Pt(3, 4);
        Check(p.X + p.Y == 7, "struct");
        decimal d = 1.1m + 2.2m;
        Check(d == 3.3m, "decimal");
        Guid g = Guid.NewGuid();
        Check(g != Guid.Empty, "guid");
    }

    static void T08_FileIO() {
        string dir = "ux0:/data/monoapp/t/";
        Directory.CreateDirectory(dir);
        Check(Directory.Exists(dir), "dir-create");
        string f = dir + "a.txt";
        File.WriteAllText(f, "hello\nworld\n");
        Check(File.Exists(f), "file-exists");
        string[] lines = File.ReadAllLines(f);
        Check(lines.Length == 2 && lines[1] == "world", "read-lines");
        byte[] b = File.ReadAllBytes(f);
        Check(b.Length == 12, "read-bytes");
        File.AppendAllText(f, "!");
        Check(new FileInfo(f).Length == 13, "append");
        string[] files = Directory.GetFiles(dir);
        Check(files.Length == 1, "getfiles");
        File.Delete(f);
        Check(!File.Exists(f), "delete");
        Directory.Delete(dir);
        Check(!Directory.Exists(dir), "dir-delete");
        Check(Path.Combine("a", "b") == "a/b", "path-combine");
    }

    static void T09_Arrays() {
        int[,] m = new int[3, 4];
        m[2, 3] = 99;
        Check(m[2, 3] == 99 && m.GetLength(0) == 3, "multidim");
        int[][] j = new int[2][];
        j[0] = new int[] { 1, 2 }; j[1] = new int[] { 3 };
        Check(j[0][1] + j[1][0] == 5, "jagged");
        int[] s = new int[] { 5, 3, 8, 1 };
        Array.Sort(s);
        Check(s[0] == 1 && s[3] == 8, "sort");
        int idx = Array.IndexOf(s, 5);
        Check(idx == 2, "indexof");
        Array.Reverse(s);
        Check(s[0] == 8, "reverse");
    }

    static void T10_Math() {
        Check(Math.Abs(-5) == 5, "abs");
        Check(Math.Max(3, 9) == 9 && Math.Min(3, 9) == 3, "minmax");
        double s = Math.Sqrt(144.0);
        Check(s == 12.0, "sqrt");
        Check(Math.Floor(2.9) == 2.0 && Math.Ceiling(2.1) == 3.0, "floor-ceil");
        Check(Math.Pow(2.0, 10.0) == 1024.0, "pow");
        double sn = Math.Sin(Math.PI / 2.0);
        Check(sn > 0.9999 && sn < 1.0001, "sin");
        Check(Math.Round(2.5) == 2.0, "round-bank");
    }

    static void T11_Encoding() {
        byte[] utf8 = Encoding.UTF8.GetBytes("héllo→");
        string back = Encoding.UTF8.GetString(utf8);
        Check(back == "héllo→", "utf8-roundtrip");
        byte[] asc = Encoding.ASCII.GetBytes("abc");
        Check(asc.Length == 3 && asc[0] == 97, "ascii");
        string b64 = Convert.ToBase64String(new byte[] { 1, 2, 3 });
        Check(b64 == "AQID", "base64");
        Check(Convert.FromBase64String(b64).Length == 3, "base64-back");
    }

    static void T12_Collections() {
        Queue<int> q = new Queue<int>();
        q.Enqueue(1); q.Enqueue(2);
        Check(q.Dequeue() == 1 && q.Count == 1, "queue");
        Stack<string> st = new Stack<string>();
        st.Push("a"); st.Push("b");
        Check(st.Pop() == "b", "stack");
        HashSet<int> hs = new HashSet<int>();
        hs.Add(1); hs.Add(1); hs.Add(2);
        Check(hs.Count == 2, "hashset");
        ArrayList al = new ArrayList();
        al.Add("x"); al.Add(42);
        Check(al.Count == 2 && (int)al[1] == 42, "arraylist");
        Hashtable ht = new Hashtable();
        ht["k"] = "v";
        Check((string)ht["k"] == "v", "hashtable");
    }

    static void T13_Strings() {
        Check(string.Format("{0}-{1}", "a", 42) == "a-42", "format");
        string[] parts = "a,b,c".Split(',');
        Check(parts.Length == 3 && parts[2] == "c", "split");
        Check(string.Join(";", parts) == "a;b;c", "join");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 50; i++) sb.Append(i);
        Check(sb.Length > 50, "stringbuilder");
        Check("  pad  ".Trim() == "pad", "trim");
        Check("hello".Substring(1, 3) == "ell", "substr");
        Check("Hello".ToLower() == "hello", "tolower");
        Check(string.Format("{0:X}", 255) == "FF", "format-hex");
    }

    static void T14_DateTime() {
        DateTime d = new DateTime(2026, 9, 10, 12, 30, 0);
        Check(d.Year == 2026 && d.Month == 9 && d.Day == 10, "dt-parts");
        DateTime d2 = d.AddDays(5);
        Check(d2.Day == 15, "dt-add");
        TimeSpan ts = d2 - d;
        Check(ts.TotalDays == 5.0, "timespan");
        Check(DateTime.IsLeapYear(2024) && !DateTime.IsLeapYear(2025), "leap");
    }

    static void T15_Boxing() {
        object o = 42;
        Check((int)o == 42, "box-unbox");
        object s = "str";
        Check((string)s == "str", "box-str");
        int? n = 7;
        object on = n;
        Check((int)on == 7, "box-nullable");
        Check(o.GetType() == typeof(int), "gettype-boxed");
        Check(5 is int && "x" is string, "is-op");
    }
}
