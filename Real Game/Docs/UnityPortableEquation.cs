// Unity C# adapter consuming the same portable equation format.
using System;
[Serializable]
public class PortableEquation { public int version = 1; public float a, b, c; }
public static class PortableEquationEval {
    public static float Eval(PortableEquation e, float x) => e.a*x*x + e.b*x + e.c;
}
