from flask import Flask, request
from flask_socketio import SocketIO
import torch
from PIL import Image
import numpy as np
import cv2

processed_frame = None
app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

# Load YOLOv5 model from local path
model_path = 'F:/yolov5-7.0'
model = torch.hub.load(model_path, 'custom', path=model_path + '/yolov5s.pt', source='local', force_reload=True)

@app.route('/')
def index():
    return "Server is running"

@app.route('/get_processed_frame', methods=['GET'])
def get_processed_frame():
    global processed_frame
    if processed_frame is None:
        app.logger.info("No processed frame available to send.")
        return '', 204  # No content
    app.logger.info("Sending processed frame.")
    _, img_encoded = cv2.imencode('.jpg', processed_frame)
    return img_encoded.tobytes(), 200, {'Content-Type': 'image/jpeg'}

@app.route('/upload', methods=['POST'])
def upload_frame():
    global processed_frame
    try:
        frame = request.data
        if not frame:
            raise ValueError("No frame data received")

        # Convert bytes data to image
        npimg = np.frombuffer(frame, np.uint8)
        img = cv2.imdecode(npimg, cv2.IMREAD_COLOR)
        if img is None:
            raise ValueError("Failed to decode image")

        # Convert BGR to RGB
        img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

        # Convert to PIL Image
        img_pil = Image.fromarray(img_rgb)

        # Perform detection
        results = model(img_pil, size=640)

        # Check results
        results_df = results.pandas().xyxy[0]
        print(results_df)  # Print the detection results for debugging

        # Filter out low confidence results
        results_df = results_df[results_df['confidence'] > 0.25]
        if results_df.empty:
            print("No detections with sufficient confidence.")
            return '', 200

        # Draw rectangles on the image
        for index, row in results_df.iterrows():
            xmin, ymin, xmax, ymax = int(row['xmin']), int(row['ymin']), int(row['xmax']), int(row['ymax'])
            label = f"{row['name']} {row['confidence']:.2f}"
            cv2.rectangle(img, (xmin, ymin), (xmax, ymax), (0, 255, 0), 2)
            cv2.putText(img, label, (xmin, ymin - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

        processed_frame = img
        app.logger.info("Processed frame updated.")

        # Display image in server window
        cv2.imshow('YOLOv5 Detection', img)
        cv2.waitKey(1)  # 1 millisecond delay for the window to update

        return '', 200
    except Exception as e:
        app.logger.error(f"Error uploading frame: {e}")
        return '', 500

@socketio.on('connect')
def handle_connect():
    print('Client connected')
    app.logger.info('Client connected')

@socketio.on('disconnect')
def handle_disconnect():
    print('Client disconnected')
    app.logger.info('Client disconnected')

if __name__ == '__main__':
    socketio.run(app, debug=True, host='0.0.0.0', port=5000)