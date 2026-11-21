// ui.Native's behavior on the Mac: a native view kind the host registers
// (nativeFactories: the host's own, by kind, which modules add), else a
// package's typed view (its module's class # its name: NativeModules.swift's
// AsukaNative.makeView). The node's view is an AsukaView holding the
// native view's, which fills it as the core places it. Its props go to the
// view's set, its events come back with the context's emit, and it's
// measured by its sizeThatFits (AsukaSizing's, or its view's fittingSize).
import AppKit
#if canImport(AsukaOps)
import AsukaOps
#endif

@MainActor extension AsukaRuntime {
    /// Registers one of the host's native view kinds for ui.Native.
    func registerNative(_ type: String, _ make: @escaping (AsukaNativeContext) -> AsukaNativeView) {
        nativeFactories[type] = make
    }

    /// The factory of a kind: the host's own (a module's, registered as the
    /// modules are, the first time one's needed).
    func nativeFactory(_ type: String) -> ((AsukaNativeContext) -> AsukaNativeView)? {
        if let f = nativeFactories[type] { return f }
        if !modulesRegistered {
            modulesRegistered = true
            registerAllModules()
        }
        if let f = nativeFactories[type] { return f }
        return nativeTestFactory(type)
    }

    /// Sets a Native node's Type (making its view) or one of its props.
    func setNativeProp(_ n: AsukaNode, _ p: UInt16, _ v: AsukaValue) {
        switch p {
        case AsukaProp.Native.type:
            guard n.native == nil else { return }
            let h = AsukaNativeHolder()
            h.type = v.string
            n.native = h
            let ctx = AsukaNativeContext(id: n.id)
            guard let nv = nativeFactory(h.type)?(ctx) ?? AsukaNative.makeView(h.type, ctx) else {
                FileHandle.standardError.write(Data("asuka: no native view \"\(h.type)\": build the app with its packages' native code\n".utf8))
                h.pending = []
                return
            }
            h.native = nv
            if let host = n.view {
                let view = nv.view
                view.frame = host.bounds
                view.autoresizingMask = [.width, .height]
                host.autoresizesSubviews = true
                host.addSubview(view)
            }
            for (name, value) in h.pending { nv.set(name, value) }
            h.pending = []
        case AsukaProp.Native.prop:
            guard let h = n.native, let (name, value) = asukaNativeProp(v.data) else { return }
            if let nv = h.native {
                nv.set(name, value)
            } else if !h.type.isEmpty {
                h.pending.append((name, value))
            }
            if n.placed { n.view?.needsLayout = true }
        default:
            break
        }
    }

    /// A native view's size for the layout's offer (NaN: as wide or tall as
    /// it likes).
    func measureNative(_ n: AsukaNode, _ w: Float, _ h: Float) -> CGSize {
        guard let nv = n.native?.native else { return .zero }
        let offer = CGSize(width: w.isNaN ? .greatestFiniteMagnitude : CGFloat(w), height: h.isNaN ? .greatestFiniteMagnitude : CGFloat(h))
        if let s = nv as? AsukaSizing { return s.sizeThatFits(offer) }
        return nv.sizeThatFits(offer)
    }

    /// Lets the native view go, as the core destroys its node.
    func destroyNative(_ n: AsukaNode) {
        guard let h = n.native, let nv = h.native else { return }
        if let d = nv as? AsukaDestroying { d.destroy() } else { nv.destroy() }
        h.native = nil
    }

    func nativeJSON(_ n: AsukaNode, _ h: AsukaNativeHolder, _ d: inout [String: Any]) {
        var j: [String: Any] = ["type": h.type, "made": h.native != nil]
        if let v = h.native?.view {
            j["viewClass"] = String(describing: type(of: v))
            j["viewFrame"] = [Double(v.frame.minX), Double(v.frame.minY), Double(v.frame.width), Double(v.frame.height)]
            j["inHost"] = v.superview === n.view
            if let s = v as? NSSlider { j["doubleValue"] = s.doubleValue }
        }
        if let t = h.native as? AsukaTestNative { j["set"] = t.set }
        d["native"] = j
    }
}

/// A Native prop's bytes: string name, u8 type (1 bool, 2 int, 3 float,
/// 4 string, 5 a typed view's encoded values), the value.
func asukaNativeProp(_ b: [UInt8]) -> (String, AsukaNativeValue)? {
    var r = AsukaByteReader(b)
    let name = r.string()
    let t = r.u8()
    let value: AsukaNativeValue
    switch t {
    case 1: value = .bool(r.u8() != 0)
    case 2: value = .int(Int64(bitPattern: r.u64()))
    case 3: value = .float(r.f64())
    case 5: value = .bytes(Data(r.bytes()))
    default: value = .string(r.string())
    }
    return r.bad ? nil : (name, value)
}
