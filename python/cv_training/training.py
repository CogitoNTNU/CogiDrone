from ultralytics import YOLO

#Load pretrained model
model = YOLO("yolo26n.pt") 

#trains model with data.yaml from personens seen from above
results = model.train(data="python/Datasett/search-and-rescue/data.yaml", epochs=100, imgsz=640)

#Validitating with best.pt on val-datasett
metrics = model.val()
print("===== Valideringsresultater =====")
print(f"Nøyaktighet (mAP50-95): {metrics.box.map * 100:.1f}%   <- strengeste mål")
print(f"Nøyaktighet (mAP50):    {metrics.box.map50 * 100:.1f}%   <- mer lowkey mål")
print(f"Precision:              {metrics.box.mp * 100:.1f}%   <- hvor mange av deteksjonene var riktige")
print(f"Recall:                 {metrics.box.mr * 100:.1f}%   <- hvor mange av personene ble faktisk funnet")

#Validitating with test which it have never seen before
test_metrics = model.val(split="test")
print("Test mAP50-95:", test_metrics.box.map)
print("Test mAP50:", test_metrics.box.map50)
