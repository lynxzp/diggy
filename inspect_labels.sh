#!/bin/bash
# Script to inspect Docker image labels
# Usage: ./inspect_labels.sh <image-name>

IMAGE_NAME="${1:-diggy}"

echo "Inspecting Docker image labels for: $IMAGE_NAME"
echo "=============================================="

if docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "Image size labels:"
    docker image inspect "$IMAGE_NAME" --format '{{range $k, $v := .Config.Labels}}{{if or (contains $k "image.size") (contains $k "org.opencontainers.image")}}{{$k}}: {{$v}}{{println}}{{end}}{{end}}'
    
    echo ""
    echo "Image size on disk:"
    docker image ls "$IMAGE_NAME" --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}"
else
    echo "Error: Image '$IMAGE_NAME' not found. Build it first with:"
    echo "  docker build -t $IMAGE_NAME ."
fi