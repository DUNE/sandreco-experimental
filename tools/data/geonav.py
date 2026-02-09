import sys
import array
import math
import ROOT

def scan_path(root_path):
    global f
    global g
    g.cd(root_path)
    n = g.GetCurrentNode()

    local = array.array('d', [0.] * 3)
    master = array.array('d', [0.] * 3)

    for i in range(n.GetNdaughters()):
        for j in range(n.GetDaughter(i).GetNdaughters()):
            path = f"{root_path}/{n.GetDaughter(i).GetName()}/{n.GetDaughter(i).GetDaughter(j).GetName()}"
            g.cd(path)
            local[0], local[1], local[2] = 0., 0., 0.
            shape = n.GetDaughter(i).GetDaughter(j).GetVolume().GetShape()
            if shape.TestShapeBit(ROOT.TGeoShape.EShapeType.kGeoTubeSeg) == True:
                local[0], local[1], local[2] = 0.5 * (shape.GetRmin() + shape.GetRmax()) * math.cos(shape.GetPhi1()/180.*ROOT.TMath.Pi()), 0.5 * (shape.GetRmin() + shape.GetRmax()) * math.sin(shape.GetPhi1()/180.*ROOT.TMath.Pi()), 0
                g.LocalToMaster(local,master)
                print(f"  {local[0]}, {local[1]}, {local[2]}, {master[0]}, {master[1]}, {master[2]}, {path}")
                local[0], local[1], local[2] = 0.5 * (shape.GetRmin() + shape.GetRmax()) * math.cos(shape.GetPhi2()/180.*ROOT.TMath.Pi()), 0.5 * (shape.GetRmin() + shape.GetRmax()) * math.sin(shape.GetPhi2()/180.*ROOT.TMath.Pi()), 0
                g.LocalToMaster(local,master)
                print(f"  {local[0]}, {local[1]}, {local[2]}, {master[0]}, {master[1]}, {master[2]}, {path}")
            else:
                local[0], local[1], local[2] = 0., -shape.GetDY(), 0.
                g.LocalToMaster(local,master)
                print(f"  {local[0]}, {local[1]}, {local[2]}, {master[0]}, {master[1]}, {master[2]}, {path}")
                local[0], local[1], local[2] = 0., shape.GetDY(), 0.
                g.LocalToMaster(local,master)
                print(f"  {local[0]}, {local[1]}, {local[2]}, {master[0]}, {master[1]}, {master[2]}, {path}")

if __name__ == "__main__":
    fname = sys.argv[1]
    f = ROOT.TFile(fname)
    g = f.Get("EDepSimGeometry")
    root_path = "/volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/kloe_calo_volume_PV_0/ECAL_endcap_lv_PV_0"
    scan_path(root_path)
    root_path = "/volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/kloe_calo_volume_PV_0/ECAL_endcap_lv_PV_1"
    scan_path(root_path)

# e1 = n.GetDaughter(24)
# e2 = n.GetDaughter(25)

# for i in range(e1.GetNdaughters()):
#     nn = e1.GetDaughter(i).GetDaughter(0)
#     name = nn.GetName()
#     sh = nn.GetVolume().GetShape()
#     print(f"{i} - [{int(sh.GetDX()*2/44.4)}] {sh.GetDX()}, {sh.GetDY()}, {sh.GetDZ()} -- {name} / {e1.GetDaughter(i).GetName()}")
    
# 0 - [6] 133.2, 510.0, 127.5 -- ECAL_ec_mod_vert_0_lv_PV_0 / ECAL_ec_mod_0_lv_PV_0
# 1 - [6] 133.2, 510.0, 127.5 -- ECAL_ec_mod_vert_0_lv_PV_0 / ECAL_ec_mod_0_lv_PV_1
# 2 - [6] 133.2, 510.0, 127.5 -- ECAL_ec_mod_vert_1_lv_PV_0 / ECAL_ec_mod_1_lv_PV_0
# 3 - [6] 133.2, 510.0, 127.5 -- ECAL_ec_mod_vert_1_lv_PV_0 / ECAL_ec_mod_1_lv_PV_1
# 4 - [3] 66.6, 1605.0, 127.5 -- ECAL_ec_mod_vert_2_lv_PV_0 / ECAL_ec_mod_2_lv_PV_0
# 5 - [3] 66.6, 1605.0, 127.5 -- ECAL_ec_mod_vert_2_lv_PV_0 / ECAL_ec_mod_2_lv_PV_1
# 6 - [3] 66.6, 1573.0, 127.5 -- ECAL_ec_mod_vert_3_lv_PV_0 / ECAL_ec_mod_3_lv_PV_0
# 7 - [3] 66.6, 1573.0, 127.5 -- ECAL_ec_mod_vert_3_lv_PV_0 / ECAL_ec_mod_3_lv_PV_1
# 8 - [3] 66.6, 1532.0, 127.5 -- ECAL_ec_mod_vert_4_lv_PV_0 / ECAL_ec_mod_4_lv_PV_0
# 9 - [3] 66.6, 1532.0, 127.5 -- ECAL_ec_mod_vert_4_lv_PV_0 / ECAL_ec_mod_4_lv_PV_1
# 10 - [3] 66.6, 1479.0, 127.5 -- ECAL_ec_mod_vert_5_lv_PV_0 / ECAL_ec_mod_5_lv_PV_0
# 11 - [3] 66.6, 1479.0, 127.5 -- ECAL_ec_mod_vert_5_lv_PV_0 / ECAL_ec_mod_5_lv_PV_1
# 12 - [3] 66.6, 1415.0, 127.5 -- ECAL_ec_mod_vert_6_lv_PV_0 / ECAL_ec_mod_6_lv_PV_0
# 13 - [3] 66.6, 1415.0, 127.5 -- ECAL_ec_mod_vert_6_lv_PV_0 / ECAL_ec_mod_6_lv_PV_1
# 14 - [3] 66.6, 1338.0, 127.5 -- ECAL_ec_mod_vert_7_lv_PV_0 / ECAL_ec_mod_7_lv_PV_0
# 15 - [3] 66.6, 1338.0, 127.5 -- ECAL_ec_mod_vert_7_lv_PV_0 / ECAL_ec_mod_7_lv_PV_1
# 16 - [3] 66.6, 1246.0, 127.5 -- ECAL_ec_mod_vert_8_lv_PV_0 / ECAL_ec_mod_8_lv_PV_0
# 17 - [3] 66.6, 1246.0, 127.5 -- ECAL_ec_mod_vert_8_lv_PV_0 / ECAL_ec_mod_8_lv_PV_1
# 18 - [3] 66.6, 1137.0, 127.5 -- ECAL_ec_mod_vert_9_lv_PV_0 / ECAL_ec_mod_9_lv_PV_0
# 19 - [3] 66.6, 1137.0, 127.5 -- ECAL_ec_mod_vert_9_lv_PV_0 / ECAL_ec_mod_9_lv_PV_1
# 20 - [3] 66.6, 1007.0, 127.5 -- ECAL_ec_mod_vert_10_lv_PV_0 / ECAL_ec_mod_10_lv_PV_0
# 21 - [3] 66.6, 1007.0, 127.5 -- ECAL_ec_mod_vert_10_lv_PV_0 / ECAL_ec_mod_10_lv_PV_1
# 22 - [3] 66.6, 845.0, 127.5 -- ECAL_ec_mod_vert_11_lv_PV_0 / ECAL_ec_mod_11_lv_PV_0
# 23 - [3] 66.6, 845.0, 127.5 -- ECAL_ec_mod_vert_11_lv_PV_0 / ECAL_ec_mod_11_lv_PV_1
# 24 - [2] 44.400000000000006, 716.0, 127.5 -- ECAL_ec_mod_vert_12_lv_PV_0 / ECAL_ec_mod_12_lv_PV_0
# 25 - [2] 44.400000000000006, 716.0, 127.5 -- ECAL_ec_mod_vert_12_lv_PV_0 / ECAL_ec_mod_12_lv_PV_1
# 26 - [2] 44.400000000000006, 554.0, 127.5 -- ECAL_ec_mod_vert_13_lv_PV_0 / ECAL_ec_mod_13_lv_PV_0
# 27 - [2] 44.400000000000006, 554.0, 127.5 -- ECAL_ec_mod_vert_13_lv_PV_0 / ECAL_ec_mod_13_lv_PV_1
# 28 - [2] 44.400000000000006, 357.0, 127.5 -- ECAL_ec_mod_vert_14_lv_PV_0 / ECAL_ec_mod_14_lv_PV_0
# 29 - [2] 44.400000000000006, 357.0, 127.5 -- ECAL_ec_mod_vert_14_lv_PV_0 / ECAL_ec_mod_14_lv_PV_1
# 30 - [2] 44.400000000000006, 43.0, 127.5 -- ECAL_ec_mod_vert_15_lv_PV_0 / ECAL_ec_mod_15_lv_PV_0
# 31 - [2] 44.400000000000006, 43.0, 127.5 -- ECAL_ec_mod_vert_15_lv_PV_0 / ECAL_ec_mod_15_lv_PV_1