# The site for Cloud Run. Built from the folder deploy.ts stages, not the repo:
# that holds gigabytes of build output the image has no use for.
FROM nginx:1.27-alpine
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY site/ /usr/share/nginx/html/
