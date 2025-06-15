# BE RESEAU
## TPs BE Reseau - 3 MIC

## Versions
Nous avons implémenté les 4 versions de mictcp, jusqu'au v4.2.

## Choix d'implémentation
- Fiabilité partielle : Pour implémenter la fiabilité partielle du v3, nous avons parti du principe d'une fenêtre glissante, un tableau fg de taille 10. 

- Négociation des pertes : Ici, nous avons choisi d'envoyer la proposition de max_perte côté serveur dans le même pdu que le SYNACK, et celui du client par ACK (après avoir fait un choix duquel à choisir). Nous avons décidé que la proposition du client va être choisi si c'est inférieure à celui du serveur, sinon nous prenons la moyenne des deux. Comme c'est le client qui demande un service, nous pensons que c'est juste qu'il peut avoir plus d'impact sur le choix de max_perte et avoir le dernier mot, tout en gardant un possibilité pour le serveur de partager son avis.
